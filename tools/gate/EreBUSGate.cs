/*
 * EreBUS Gate -- a Windows front-end that packages a task and feeds it into
 * an EreBUS node over ssh, where the desk distributes it among the machines.
 *
 * Builds a .ebtask package (a "key | value" manifest, a "--" line, then the
 * payload) from the fields and the editor, and pipes it into the node's
 * terminal over the built-in OpenSSH client: "receive <n> bytes as job", the
 * package, "submit job", then reads the task back so the folded result shows.
 *
 * No SDK: build with the .NET Framework compiler.
 *   tools\gate\build.cmd            -> build\gate\EreBUS-Gate.exe
 * No arguments opens the window; --headless drives the core from a command
 * line; --shot <file.png> renders the window to an image; "api <verb>" is the
 * machine interface (one json object on stdout), and "api watch" streams json
 * events, one per line.
 */
using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("EreBUS Gate")]
[assembly: System.Reflection.AssemblyProduct("EreBUS Gate")]
[assembly: System.Reflection.AssemblyCompany("github.com/DustinHab/EreBUS")]
[assembly: System.Reflection.AssemblyVersion("0.1.3.0")]
[assembly: System.Reflection.AssemblyFileVersion("0.1.3.0")]

namespace EreBUSGate
{
    static class Ver { public const string V = "0.1.3"; }

    static class Look
    {
        public static readonly Color Ground = Color.FromArgb(0x16, 0x13, 0x10);
        public static readonly Color Panel  = Color.FromArgb(0x1F, 0x1B, 0x16);
        public static readonly Color Field  = Color.FromArgb(0x24, 0x1F, 0x19);
        public static readonly Color Ink    = Color.FromArgb(0xDD, 0xD6, 0xC8);
        public static readonly Color Dim    = Color.FromArgb(0x8C, 0x84, 0x74);
        public static readonly Color Faint  = Color.FromArgb(0x5E, 0x57, 0x4B);
        public static readonly Color Accent = Color.FromArgb(0xC8, 0x5A, 0x28);
        public static readonly Color WarnC  = Color.FromArgb(0xD8, 0x9A, 0x3A);
        public static readonly Color Edge   = Color.FromArgb(0x39, 0x33, 0x2B);
        public static Font Body  = new Font("Consolas", 10.5f, FontStyle.Regular, GraphicsUnit.Point);
        public static Font Mono  = new Font("Consolas", 9.5f,  FontStyle.Regular, GraphicsUnit.Point);
        public static Font Small = new Font("Consolas", 8.25f, FontStyle.Regular, GraphicsUnit.Point);
        public static Font Head  = new Font("Consolas", 22f,   FontStyle.Bold,    GraphicsUnit.Point);
        public static Font Res   = new Font("Consolas", 12f,   FontStyle.Bold,    GraphicsUnit.Point);

        public static string Spaced(string s)
        {
            var sb = new StringBuilder();
            for (int i = 0; i < s.Length; i++) { if (i > 0) sb.Append(' '); sb.Append(char.ToUpperInvariant(s[i])); }
            return sb.ToString();
        }
    }

    class Spec
    {
        public string Source, Node, Key, Combine, InputName, Name;
        public int Port = 22, Pieces, Across, Budget, Wait = 8;
        public bool HasSplit, Recipe;
        public long SplitLo, SplitHi;
    }

    class NodeEntry
    {
        public string Name = "", Host = "", Key = "";
        public int Port = 22;
        public override string ToString() { return (Name.Length > 0 ? Name : Host); }
    }

    static class Store
    {
        static string Dir()
        {
            string d = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "EreBUS Gate");
            try { Directory.CreateDirectory(d); } catch { }
            return d;
        }
        static string File_() { return Path.Combine(Dir(), "gate.ini"); }

        public static void Save(List<NodeEntry> nodes, Dictionary<string, string> last)
        {
            try
            {
                var sb = new StringBuilder();
                foreach (var n in nodes)
                    sb.Append("node|").Append(Esc(n.Name)).Append('|').Append(Esc(n.Host)).Append('|')
                      .Append(n.Port).Append('|').Append(Esc(n.Key)).Append('\n');
                foreach (var kv in last)
                    sb.Append("last|").Append(Esc(kv.Key)).Append('|').Append(Esc(kv.Value)).Append('\n');
                File.WriteAllText(File_(), sb.ToString(), new UTF8Encoding(false));
            }
            catch { }
        }

        public static void Load(List<NodeEntry> nodes, Dictionary<string, string> last)
        {
            try
            {
                if (!File.Exists(File_())) return;
                foreach (var line in File.ReadAllLines(File_()))
                {
                    string[] p = line.Split('|');
                    if (p.Length >= 5 && p[0] == "node")
                    {
                        var n = new NodeEntry(); n.Name = Un(p[1]); n.Host = Un(p[2]);
                        int.TryParse(p[3], out n.Port); n.Key = Un(p[4]); nodes.Add(n);
                    }
                    else if (p.Length >= 3 && p[0] == "last") last[Un(p[1])] = Un(p[2]);
                }
            }
            catch { }
        }
        static string Esc(string s) { return (s ?? "").Replace("\\", "\\\\").Replace("|", "\\p").Replace("\n", " "); }
        static string Un(string s) { return (s ?? "").Replace("\\p", "|").Replace("\\\\", "\\"); }
    }

    static class Core
    {
        public const int WireMax = 1024;   /* RECIPE_MAX in kernel/net/pipe.c */
        public const int SlotMax = 8;       /* PART_MAX */

        static byte[] NormalizeLf(byte[] d)
        {
            var ms = new MemoryStream(d.Length);
            for (int i = 0; i < d.Length; i++)
            {
                if (d[i] == 0x0D) { ms.WriteByte(0x0A); if (i + 1 < d.Length && d[i + 1] == 0x0A) i++; }
                else ms.WriteByte(d[i]);
            }
            return ms.ToArray();
        }

        public static string Manifest(Spec s)
        {
            var m = new StringBuilder();
            string name = !string.IsNullOrEmpty(s.Name) ? s.Name
                        : (!string.IsNullOrEmpty(s.Source) ? Path.GetFileName(s.Source) : "task");
            m.Append("task | ").Append(name).Append('\n');
            m.Append("kind | ").Append(s.Recipe ? "recipe" : "code").Append('\n');
            if (s.HasSplit) m.Append("split | ").Append(s.SplitLo).Append(' ').Append(s.SplitHi).Append('\n');
            if (s.Pieces > 0) m.Append("pieces | ").Append(s.Pieces).Append('\n');
            if (s.Across > 0) m.Append("across | ").Append(s.Across).Append('\n');
            if (!string.IsNullOrEmpty(s.Combine)) m.Append("combine | ").Append(s.Combine).Append('\n');
            if (s.Budget > 0) m.Append("budget | ").Append(s.Budget).Append('\n');
            if (!string.IsNullOrEmpty(s.InputName)) m.Append("input | ").Append(s.InputName).Append('\n');
            m.Append("--\n");
            return m.ToString();
        }

        public static byte[] Package(Spec s, byte[] payload)
        {
            payload = NormalizeLf(payload);
            byte[] manifest = new UTF8Encoding(false).GetBytes(Manifest(s));
            var ms = new MemoryStream();
            ms.Write(manifest, 0, manifest.Length);
            ms.Write(payload, 0, payload.Length);
            if (payload.Length == 0 || payload[payload.Length - 1] != (byte)'\n') ms.WriteByte((byte)'\n');
            return ms.ToArray();
        }

        public static byte[] BuildPackage(Spec s, out string err)
        {
            err = null;
            byte[] payload;
            try { payload = File.ReadAllBytes(s.Source); }
            catch (Exception e) { err = "cannot read source: " + e.Message; return null; }
            return Package(s, payload);
        }

        static void Write(Stream st, string ascii) { byte[] b = Encoding.ASCII.GetBytes(ascii); st.Write(b, 0, b.Length); }

        static Process StartSsh(Spec s, Action<string> log)
        {
            var psi = new ProcessStartInfo();
            psi.FileName = "ssh.exe";
            var a = new StringBuilder();
            a.Append("-T ");
            if (s.Port > 0 && s.Port != 22) a.Append("-p ").Append(s.Port).Append(' ');
            if (!string.IsNullOrEmpty(s.Key)) a.Append("-i \"").Append(s.Key).Append("\" ");
            a.Append("-o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 ");
            a.Append(s.Node);
            psi.Arguments = a.ToString();
            psi.UseShellExecute = false; psi.RedirectStandardInput = true;
            psi.RedirectStandardOutput = true; psi.RedirectStandardError = true; psi.CreateNoWindow = true;
            Process p;
            try { p = Process.Start(psi); } catch (Exception e) { log("ssh could not start: " + e.Message); return null; }
            p.OutputDataReceived += delegate(object o, DataReceivedEventArgs e) { if (e.Data != null) log(e.Data); };
            p.ErrorDataReceived  += delegate(object o, DataReceivedEventArgs e) { if (e.Data != null) log(e.Data); };
            p.BeginOutputReadLine(); p.BeginErrorReadLine();
            return p;
        }

        public static int Feed(Spec s, byte[] pkg, Action<string> log)
        {
            Process p = StartSsh(s, log); if (p == null) return -1;
            try
            {
                var stdin = p.StandardInput.BaseStream;
                Write(stdin, "receive " + pkg.Length + " bytes as job\n");
                stdin.Write(pkg, 0, pkg.Length);
                Write(stdin, "submit job\n"); stdin.Flush();
                if (s.Wait > 0)
                {
                    log("... waiting " + s.Wait + "s");
                    Thread.Sleep(s.Wait * 1000);
                    Write(stdin, "read job\n"); stdin.Flush();
                    Thread.Sleep(1500);
                }
                stdin.Close();
            }
            catch (Exception e) { log("wire broke: " + e.Message); }
            if (!p.WaitForExit(25000)) { try { p.Kill(); } catch { } }
            return p.HasExited ? p.ExitCode : 0;
        }

        public static int Probe(Spec s, Action<string> log)
        {
            Process p = StartSsh(s, log); if (p == null) return -1;
            try { Write(p.StandardInput.BaseStream, "version\nwhere\n"); p.StandardInput.BaseStream.Flush(); p.StandardInput.Close(); }
            catch (Exception e) { log("wire broke: " + e.Message); }
            if (!p.WaitForExit(15000)) { try { p.Kill(); } catch { } }
            return p.HasExited ? p.ExitCode : 0;
        }

        public static int Scan(Spec s, Action<string> log)
        {
            Process p = StartSsh(s, log); if (p == null) return -1;
            try
            {
                var stdin = p.StandardInput.BaseStream;
                Write(stdin, "scan\n"); stdin.Flush();
                Thread.Sleep(4000);          /* a scan calls out for a few seconds */
                Write(stdin, "found\nnodes\n"); stdin.Flush();
                Thread.Sleep(1200);
                stdin.Close();
            }
            catch (Exception e) { log("wire broke: " + e.Message); }
            if (!p.WaitForExit(20000)) { try { p.Kill(); } catch { } }
            return p.HasExited ? p.ExitCode : 0;
        }

        public static int Read(Spec s, string name, Action<string> log)
        {
            Process p = StartSsh(s, log); if (p == null) return -1;
            try
            {
                var stdin = p.StandardInput.BaseStream;
                Write(stdin, "read " + name + "\n"); stdin.Flush();
                Thread.Sleep(s.Wait > 0 ? s.Wait * 1000 : 1500);
                stdin.Close();
            }
            catch (Exception e) { log("wire broke: " + e.Message); }
            if (!p.WaitForExit(15000)) { try { p.Kill(); } catch { } }
            return p.HasExited ? p.ExitCode : 0;
        }

        public static string VersionOf(List<string> lines)
        {
            foreach (var l in lines)
                foreach (var t in l.Split(new char[] { ' ', '\t', ',' }, StringSplitOptions.RemoveEmptyEntries))
                {
                    string tt = t.TrimStart('v', 'V');
                    if (tt.IndexOf('.') <= 0 || tt[tt.Length - 1] == '.') continue;
                    bool ok = true; int dots = 0;
                    foreach (char c in tt) { if (c == '.') dots++; else if (c < '0' || c > '9') { ok = false; break; } }
                    if (ok && dots >= 1) return tt;
                }
            return null;
        }
    }

    class Toggle : Label
    {
        bool on; string label; public event EventHandler Changed;
        public Toggle(string text)
        {
            label = text; AutoSize = true; Font = Look.Body; Cursor = Cursors.Hand;
            Render();
            Click += delegate { on = !on; Render(); if (Changed != null) Changed(this, EventArgs.Empty); };
        }
        public bool Checked { get { return on; } set { on = value; Render(); } }
        void Render() { Text = (on ? "[x] " : "[ ] ") + label; ForeColor = on ? Look.Accent : Look.Dim; }
    }

    class GateForm : Form
    {
        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
        static extern int SetWindowTheme(IntPtr h, string app, string id);

        TextBox tSource, tNode, tKey, tLo, tHi, tPieces, tAcross, tBudget, tInputName, tName, tEditor, tPort, tWait, tResult;
        Toggle kSplit, kRecipe;
        Button[] combineBtns; string combineSel = "sum";
        ListBox nodeList;
        RichTextBox log;
        Label planLabel, gaugeLabel;
        Button send;
        ToolTip tips = new ToolTip();
        List<Control> bordered = new List<Control>();
        List<NodeEntry> nodes = new List<NodeEntry>();
        Dictionary<string, string> last = new Dictionary<string, string>();
        Point dragFrom; bool dragging; bool loading;

        public GateForm()
        {
            Store.Load(nodes, last);
            this.Text = "EreBUS Gate";
            this.FormBorderStyle = FormBorderStyle.None;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Look.Ground; this.ForeColor = Look.Ink; this.Font = Look.Body;
            this.ClientSize = new Size(1000, 760);
            this.AllowDrop = true; this.KeyPreview = true;
            this.Paint += delegate(object o, PaintEventArgs e)
            {
                var g = e.Graphics;
                using (var pen = new Pen(Look.Edge))
                {
                    g.DrawRectangle(pen, 0, 0, this.Width - 1, this.Height - 1);
                    g.DrawLine(pen, 492, 116, 492, this.Height - 16);
                    foreach (Control c in bordered)
                        if (c.Visible) g.DrawRectangle(pen, c.Left - 1, c.Top - 1, c.Width + 1, c.Height + 1);
                }
            };
            this.DragEnter += delegate(object o, DragEventArgs e) { if (e.Data.GetDataPresent(DataFormats.FileDrop)) e.Effect = DragDropEffects.Copy; };
            this.DragDrop += delegate(object o, DragEventArgs e)
            {
                string[] f = (string[])e.Data.GetData(DataFormats.FileDrop);
                if (f != null && f.Length > 0) LoadSource(f[0]);
            };
            this.KeyDown += delegate(object o, KeyEventArgs e)
            {
                if (e.KeyCode == Keys.Escape) this.Close();
                if (e.Control && e.KeyCode == Keys.Enter) DoSend();
            };
            this.FormClosing += delegate { Persist(); };

            BuildHeader();
            BuildLeft();
            BuildRight();
            LoadLast();
            RefreshPreview();
        }

        void Dark(Control c) { try { SetWindowTheme(c.Handle, "DarkMode_Explorer", null); } catch { } }

        void BuildHeader()
        {
            var head = new Panel();
            head.Bounds = new Rectangle(1, 1, this.ClientSize.Width - 2, 100);
            head.BackColor = Look.Ground;
            head.Paint += delegate(object o, PaintEventArgs e)
            {
                var g = e.Graphics;
                g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
                using (var b = new SolidBrush(Look.Accent)) g.DrawString("EreBUS Gate", Look.Head, b, 22, 20);
                using (var b = new SolidBrush(Look.Dim)) g.DrawString("v " + Ver.V, Look.Small, b, 25, 66);
                using (var pen = new Pen(Look.Edge)) g.DrawLine(pen, 22, 92, head.Width - 22, 92);
            };
            head.MouseDown += delegate(object o, MouseEventArgs e) { dragging = true; dragFrom = e.Location; };
            head.MouseUp += delegate { dragging = false; };
            head.MouseMove += delegate(object o, MouseEventArgs e)
            { if (dragging) this.Location = new Point(this.Location.X + e.X - dragFrom.X, this.Location.Y + e.Y - dragFrom.Y); };
            this.Controls.Add(head);
            head.Controls.Add(Glyph("\u00D7", head.Width - 34, 16, delegate { this.Close(); }));
            head.Controls.Add(Glyph("\u2013", head.Width - 66, 16, delegate { this.WindowState = FormWindowState.Minimized; }));
            head.Controls.Add(Glyph("?", head.Width - 98, 16, delegate { ShowAbout(); }));

            var cl = new Label();
            cl.Text = "cluster"; cl.Font = Look.Small; cl.ForeColor = Look.Dim; cl.AutoSize = true;
            cl.Location = new Point(head.Width - 172, 21); cl.Cursor = Cursors.Hand;
            cl.MouseEnter += delegate { cl.ForeColor = Look.Accent; };
            cl.MouseLeave += delegate { cl.ForeColor = Look.Dim; };
            cl.Click += delegate { ShowCluster(); };
            head.Controls.Add(cl);
        }

        void ShowCluster() { var f = new ClusterForm(nodes); try { f.Show(this); } catch { f.Show(); } }

        Label Glyph(string ch, int x, int y, Action onClick)
        {
            var l = new Label();
            l.Text = ch; l.Font = new Font("Consolas", 13f, FontStyle.Bold);
            l.ForeColor = Look.Dim; l.AutoSize = false; l.Size = new Size(24, 24);
            l.TextAlign = ContentAlignment.MiddleCenter; l.Location = new Point(x, y); l.Cursor = Cursors.Hand;
            l.MouseEnter += delegate { l.ForeColor = Look.Accent; };
            l.MouseLeave += delegate { l.ForeColor = Look.Dim; };
            l.Click += delegate { onClick(); };
            return l;
        }

        void Section(string text, int x, int y)
        {
            var l = new Label();
            l.Text = Look.Spaced(text); l.Font = Look.Small; l.ForeColor = Look.Accent;
            l.AutoSize = true; l.Location = new Point(x, y);
            this.Controls.Add(l);
            var rule = new Label(); rule.AutoSize = false; rule.BackColor = Look.Edge;
            rule.Bounds = new Rectangle(x, y + 18, (x < 490 ? 468 : 984) - x, 1);
            this.Controls.Add(rule);
        }

        Label Field(string text, int x, int y, int w)
        {
            var l = new Label();
            l.Text = text; l.Font = Look.Small; l.ForeColor = Look.Dim;
            l.AutoSize = false; l.Location = new Point(x, y); l.Size = new Size(w, 15);
            this.Controls.Add(l);
            return l;
        }

        TextBox Text_(int x, int y, int w, string tip)
        {
            var t = new TextBox();
            t.BorderStyle = BorderStyle.None; t.BackColor = Look.Field; t.ForeColor = Look.Ink;
            t.Font = Look.Body; t.Location = new Point(x + 3, y + 4); t.Size = new Size(w - 6, 20);
            var host = new Panel();
            host.BackColor = Look.Field; host.Location = new Point(x, y); host.Size = new Size(w, 26);
            host.Controls.Add(t);
            this.Controls.Add(host); bordered.Add(host);
            t.TextChanged += delegate { RefreshPreview(); };
            if (tip != null) tips.SetToolTip(t, tip);
            return t;
        }

        Button Btn(string text, int x, int y, int w, int h, bool strong)
        {
            var b = new Button();
            b.Text = text; b.Font = strong ? Look.Body : Look.Mono;
            b.FlatStyle = FlatStyle.Flat; b.FlatAppearance.BorderColor = strong ? Look.Accent : Look.Edge;
            b.FlatAppearance.BorderSize = 1; b.BackColor = Look.Ground;
            b.ForeColor = strong ? Look.Accent : Look.Dim;
            b.FlatAppearance.MouseOverBackColor = Color.FromArgb(0x2C, 0x23, 0x1B);
            b.Location = new Point(x, y); b.Size = new Size(w, h); b.Cursor = Cursors.Hand;
            b.UseCompatibleTextRendering = true;
            this.Controls.Add(b);
            return b;
        }

        void BuildLeft()
        {
            int x = 24, y = 116, w = 444;
            Section("task", x, y); y += 30;

            Field("source file", x, y, w);
            tSource = Text_(x, y + 15, w - 84, "a file to load into the editor; or type in the editor below");
            var browse = Btn("browse", x + w - 78, y + 14, 78, 26, false);
            browse.Click += delegate
            {
                var d = new OpenFileDialog(); d.Filter = "programs and recipes|*.c;*.recipe;*.txt|all files|*.*";
                if (d.ShowDialog() == DialogResult.OK) LoadSource(d.FileName);
            };
            y += 46;

            kRecipe = MkToggle("recipe", x, y, "checked: recipe for the interpreter. unchecked: c the node compiles");
            Field("name", x + 250, y - 2, 60);
            tName = Text_(x + 250, y + 12, w - 250, "task name, for the log");
            y += 44;

            kSplit = MkToggle("split", x, y, "divide lo..hi into pieces, one per machine");
            y += 26;
            Field("from", x, y, 60);      tLo = Text_(x, y + 15, 120, "low end");
            Field("to", x + 132, y, 40);  tHi = Text_(x + 132, y + 15, 120, "high end");
            Field("pieces", x + 264, y, 60); tPieces = Text_(x + 264, y + 15, 90, "count of pieces; blank = 4");
            y += 46;

            Field("combine", x, y, 120);
            string[] combos = { "sum", "concat", "min", "max", "count", "first" };
            combineBtns = new Button[combos.Length];
            int bx = x;
            for (int i = 0; i < combos.Length; i++)
            {
                string name = combos[i];
                int bw = 22 + name.Length * 9;
                var b = Btn(name, bx, y + 14, bw, 26, false);
                b.Click += delegate { SelectCombine(name); };
                combineBtns[i] = b; bx += bw + 6;
            }
            y += 48;

            Field("across", x, y, 90);  tAcross = Text_(x, y + 15, 90, "quorum: each piece on N machines, majority");
            Field("budget", x + 106, y, 90); tBudget = Text_(x + 106, y + 15, 90, "seconds per worker");
            Field("input", x + 212, y, 90); tInputName = Text_(x + 212, y + 15, w - 212, "petname of an object on the node");
            y += 46;

            planLabel = new Label(); planLabel.AutoSize = false; planLabel.Location = new Point(x, y);
            planLabel.Size = new Size(w - 120, 30); planLabel.Font = Look.Small; planLabel.ForeColor = Look.Dim;
            this.Controls.Add(planLabel);
            gaugeLabel = new Label(); gaugeLabel.AutoSize = false; gaugeLabel.Location = new Point(x + w - 116, y);
            gaugeLabel.Size = new Size(116, 15); gaugeLabel.Font = Look.Small; gaugeLabel.ForeColor = Look.Dim;
            gaugeLabel.TextAlign = ContentAlignment.TopRight; this.Controls.Add(gaugeLabel);
            y += 34;

            Field("editor  --  the payload sent to the machines", x, y, w);
            tEditor = new TextBox();
            tEditor.Multiline = true; tEditor.AcceptsTab = true; tEditor.ScrollBars = ScrollBars.Both;
            tEditor.BorderStyle = BorderStyle.None; tEditor.BackColor = Look.Panel; tEditor.ForeColor = Look.Ink;
            tEditor.Font = Look.Mono; tEditor.WordWrap = false;
            tEditor.Location = new Point(x + 2, y + 17); tEditor.Size = new Size(w - 4, this.ClientSize.Height - (y + 17) - 54);
            var ep = new Panel(); ep.BackColor = Look.Panel; ep.Location = new Point(x, y + 15);
            ep.Size = new Size(w, this.ClientSize.Height - (y + 15) - 50); ep.Controls.Add(tEditor);
            this.Controls.Add(ep); bordered.Add(ep); Dark(tEditor);
            tEditor.TextChanged += delegate { RefreshPreview(); };

            var tmplLbl = Field("templates", x, this.ClientSize.Height - 34, 80); tmplLbl.ForeColor = Look.Faint;
            var t1 = Btn("sum of a range", x + 84, this.ClientSize.Height - 38, 156, 26, false); t1.Click += delegate { LoadTemplate("sumrange"); };
            var t2 = Btn("count matches", x + 248, this.ClientSize.Height - 38, 150, 26, false); t2.Click += delegate { LoadTemplate("count"); };
        }

        void SelectCombine(string name)
        {
            combineSel = name;
            string[] combos = { "sum", "concat", "min", "max", "count", "first" };
            for (int i = 0; i < combineBtns.Length; i++)
            {
                bool on = combos[i] == name;
                combineBtns[i].ForeColor = on ? Look.Accent : Look.Dim;
                combineBtns[i].FlatAppearance.BorderColor = on ? Look.Accent : Look.Edge;
            }
            RefreshPreview();
        }

        void BuildRight()
        {
            int x = 516, y = 116, w = 460;
            Section("node", x, y); y += 30;

            Field("nodes", x, y, w);
            nodeList = new ListBox();
            nodeList.BorderStyle = BorderStyle.None; nodeList.BackColor = Look.Panel; nodeList.ForeColor = Look.Ink;
            nodeList.Font = Look.Mono; nodeList.IntegralHeight = false;
            nodeList.Location = new Point(x + 2, y + 17); nodeList.Size = new Size(w - 4, 72);
            var nlp = new Panel(); nlp.BackColor = Look.Panel; nlp.Location = new Point(x, y + 15); nlp.Size = new Size(w, 76);
            nlp.Controls.Add(nodeList); this.Controls.Add(nlp); bordered.Add(nlp); Dark(nodeList);
            nodeList.DoubleClick += delegate { LoadSelectedNode(); };
            nodeList.KeyDown += delegate(object o, KeyEventArgs e) { if (e.KeyCode == Keys.Delete) RemoveSelectedNode(); };
            RefillNodeList();
            var add = Btn("save node", x, y + 96, 108, 26, false); add.Click += delegate { SaveCurrentNode(); };
            var del = Btn("remove", x + 116, y + 96, 84, 26, false); del.Click += delegate { RemoveSelectedNode(); };
            var test = Btn("test", x + 208, y + 96, 84, 26, false); test.Click += delegate { DoProbe(); };
            var scan = Btn("scan", x + w - 90, y + 96, 90, 26, false); scan.Click += delegate { DoScan(); };
            y += 134;

            Field("host", x, y, w - 96);
            tNode = Text_(x, y + 15, w - 152, "user@host or ssh alias; its door holds your key");
            Field("port", x + w - 88, y, 88); tPort = Text_(x + w - 88, y + 15, 88, "ssh port");
            y += 46;
            Field("key", x, y, w);
            tKey = Text_(x, y + 15, w - 200, "private key file; blank = default keys / agent");
            var kb = Btn("choose", x + w - 194, y + 14, 90, 26, false);
            kb.Click += delegate { var d = new OpenFileDialog(); d.Filter = "ssh key|*|all|*.*"; if (d.ShowDialog() == DialogResult.OK) tKey.Text = d.FileName; };
            var door = Btn("door line", x + w - 98, y + 14, 98, 26, false);
            tips.SetToolTip(door, "pick your ssh public key; the 'write door | ...' line to authorise this key is copied to the clipboard");
            door.Click += delegate { DoDoorLine(); };
            y += 46;

            Field("wait", x, y, 90);
            tWait = Text_(x, y + 15, 90, "seconds to wait before reading the result; 0 = none");
            send = Btn("send", x + 116, y + 12, 200, 34, true);
            send.Click += delegate { DoSend(); };
            Btn("save package", x + 328, y + 12, w - 328, 34, false).Click += delegate { DoSave(); };
            y += 56;

            Field("result", x, y, w - 84);
            tResult = new TextBox();
            tResult.BorderStyle = BorderStyle.None; tResult.BackColor = Look.Field; tResult.ForeColor = Look.Accent;
            tResult.Font = Look.Res; tResult.ReadOnly = true;
            tResult.Location = new Point(x + 3, y + 17); tResult.Size = new Size(w - 90, 24);
            var rp = new Panel(); rp.BackColor = Look.Field; rp.Location = new Point(x, y + 15); rp.Size = new Size(w - 84, 30);
            rp.Controls.Add(tResult); this.Controls.Add(rp); bordered.Add(rp);
            var copy = Btn("copy", x + w - 78, y + 15, 78, 30, false);
            copy.Click += delegate { try { if (tResult.Text.Length > 0) Clipboard.SetText(tResult.Text); Say("result copied."); } catch { } };
            y += 50;

            Field("output", x, y, w);
            log = new RichTextBox();
            log.BorderStyle = BorderStyle.None; log.BackColor = Look.Panel; log.ForeColor = Look.Ink;
            log.Font = Look.Mono; log.ReadOnly = true; log.WordWrap = false; log.ScrollBars = RichTextBoxScrollBars.Both;
            log.Location = new Point(x + 2, y + 17); log.Size = new Size(w - 4, this.ClientSize.Height - (y + 17) - 18);
            var lp = new Panel(); lp.BackColor = Look.Panel; lp.Location = new Point(x, y + 15);
            lp.Size = new Size(w, this.ClientSize.Height - (y + 15) - 16); lp.Controls.Add(log);
            this.Controls.Add(lp); bordered.Add(lp); Dark(log);
            log.Text = "idle.\n";
        }

        Toggle MkToggle(string text, int x, int y, string tip)
        {
            var t = new Toggle(text); t.Location = new Point(x, y);
            t.Changed += delegate { RefreshPreview(); };
            if (tip != null) tips.SetToolTip(t, tip);
            this.Controls.Add(t);
            return t;
        }

        void LoadSource(string path)
        {
            loading = true;
            tSource.Text = path;
            try { tEditor.Text = File.ReadAllText(path).Replace("\r\n", "\n").Replace("\n", "\r\n"); } catch (Exception e) { Say("cannot read " + path + ": " + e.Message); }
            if (path.EndsWith(".recipe", StringComparison.OrdinalIgnoreCase)) kRecipe.Checked = true;
            if (tName.Text.Trim().Length == 0) tName.Text = Path.GetFileNameWithoutExtension(path);
            loading = false; RefreshPreview();
        }

        byte[] PayloadBytes()
        {
            string t = tEditor.Text;
            if (!string.IsNullOrEmpty(t)) return new UTF8Encoding(false).GetBytes(t);
            return new byte[0];
        }

        Spec Gather(out string err)
        {
            err = null;
            var s = new Spec();
            s.Source = tSource.Text.Trim(); s.Node = tNode.Text.Trim(); s.Key = tKey.Text.Trim();
            s.Recipe = kRecipe.Checked; s.Name = tName.Text.Trim(); s.InputName = tInputName.Text.Trim();
            s.Combine = combineSel;
            s.Across = PInt(tAcross.Text); s.Budget = PInt(tBudget.Text); s.Pieces = PInt(tPieces.Text);
            int port = PInt(tPort.Text); s.Port = (port >= 1 && port <= 65535) ? port : 22;
            int w; s.Wait = int.TryParse((tWait.Text ?? "").Trim(), out w) ? Math.Max(0, w) : 8;
            if (kSplit.Checked)
            {
                s.HasSplit = true; s.SplitLo = PLong(tLo.Text); s.SplitHi = PLong(tHi.Text);
                if (s.SplitHi < s.SplitLo) { err = "hi < lo."; return null; }
            }
            return s;
        }
        static int PInt(string t) { int v; return int.TryParse((t ?? "").Trim(), out v) ? v : 0; }
        static long PLong(string t) { long v; return long.TryParse((t ?? "").Trim(), out v) ? v : 0; }

        void RefreshPreview()
        {
            if (loading || tEditor == null) return;
            string err; var s = Gather(out err);
            if (s == null) { planLabel.ForeColor = Look.WarnC; planLabel.Text = err; return; }

            byte[] payload = PayloadBytes();
            byte[] pkg = Core.Package(s, payload);
            int bytes = pkg.Length;

            gaugeLabel.ForeColor = bytes > Core.WireMax ? Look.WarnC : Look.Dim;
            gaugeLabel.Text = bytes + " / " + Core.WireMax + " B";

            int pieces = s.HasSplit ? (s.Pieces > 0 ? s.Pieces : 4) : 1;
            int quorum = s.Across > 0 ? s.Across : 1;
            int slots = pieces * quorum;
            var plan = new StringBuilder();
            plan.Append(pieces).Append(pieces == 1 ? " piece" : " pieces");
            if (quorum > 1) plan.Append(" x ").Append(quorum).Append(" = ").Append(slots).Append(" slots");
            plan.Append("  ·  combine ").Append(s.Combine);
            var warn = new StringBuilder();
            if (bytes > Core.WireMax) warn.Append("payload > ").Append(Core.WireMax).Append(" B.  ");
            if (slots > Core.SlotMax) warn.Append(slots).Append(" slots > ").Append(Core.SlotMax).Append(".  ");
            if (warn.Length > 0) { planLabel.ForeColor = Look.WarnC; planLabel.Text = plan.ToString() + "\n" + warn.ToString(); }
            else { planLabel.ForeColor = Look.Dim; planLabel.Text = plan.ToString(); }
        }

        void Say(string line)
        {
            if (log.InvokeRequired) { log.BeginInvoke((MethodInvoker)delegate { Say(line); }); return; }
            string t = line.TrimStart();
            if (t.StartsWith("= ")) { tResult.Text = t.Substring(2).Trim(); }
            bool hot = line.StartsWith("pipe:") || t.StartsWith("= ") || line.StartsWith("---") || line.StartsWith("...");
            log.SelectionStart = log.TextLength; log.SelectionColor = hot ? Look.Accent : Look.Ink;
            log.AppendText(line + "\n"); log.SelectionStart = log.TextLength; log.ScrollToCaret();
        }

        void DoSend()
        {
            string err; var s = Gather(out err);
            if (s == null) { Say(err); return; }
            byte[] payload = PayloadBytes();
            if (payload.Length == 0) { Say("no payload (load a file or type in the editor)."); return; }
            if (string.IsNullOrEmpty(s.Node)) { Say("no node."); return; }
            byte[] pkg = Core.Package(s, payload);
            if (pkg.Length > Core.WireMax) { Say("payload " + pkg.Length + " B > " + Core.WireMax + " B; not sent."); return; }
            tResult.Text = ""; send.Enabled = false;
            Say(""); Say("--- send " + pkg.Length + " B -> " + s.Node + " ---");
            var th = new Thread(delegate()
            {
                int rc = Core.Feed(s, pkg, Say);
                Say("--- ssh exit " + rc + " ---");
                this.BeginInvoke((MethodInvoker)delegate { send.Enabled = true; });
            });
            th.IsBackground = true; th.Start();
        }

        void DoProbe()
        {
            string err; var s = Gather(out err);
            if (s == null || string.IsNullOrEmpty(s.Node)) { Say("no node."); return; }
            Say(""); Say("--- test " + s.Node + " ---");
            var th = new Thread(delegate() { int rc = Core.Probe(s, Say); Say("--- ssh exit " + rc + " ---"); });
            th.IsBackground = true; th.Start();
        }

        void DoScan()
        {
            string err; var s = Gather(out err);
            if (s == null || string.IsNullOrEmpty(s.Node)) { Say("no node."); return; }
            Say(""); Say("--- scan from " + s.Node + " ---");
            var th = new Thread(delegate() { int rc = Core.Scan(s, Say); Say("--- ssh exit " + rc + " ---"); });
            th.IsBackground = true; th.Start();
        }

        void DoDoorLine()
        {
            var d = new OpenFileDialog(); d.Filter = "ssh public key|*.pub|all|*.*";
            d.Title = "pick your ssh public key";
            if (d.ShowDialog() != DialogResult.OK) return;
            try
            {
                string pub = File.ReadAllText(d.FileName).Trim();
                string[] parts = pub.Split(new char[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length < 2) { Say("that does not look like an ssh public key."); return; }
                string line = "write door | " + parts[0] + " " + parts[1];
                Clipboard.SetText(line);
                Say("");
                Say("door line copied to the clipboard:");
                Say("  " + line);
                Say("on the node: go system, go settings, paste it, then back.");
            }
            catch (Exception e) { Say("cannot read the key: " + e.Message); }
        }

        void DoSave()
        {
            string err; var s = Gather(out err);
            if (s == null) { Say(err); return; }
            byte[] payload = PayloadBytes();
            if (payload.Length == 0) { Say("no payload."); return; }
            byte[] pkg = Core.Package(s, payload);
            var d = new SaveFileDialog(); d.Filter = "task package|*.ebtask|all|*.*";
            d.FileName = (string.IsNullOrEmpty(s.Name) ? "task" : s.Name) + ".ebtask";
            if (d.ShowDialog() == DialogResult.OK) { File.WriteAllBytes(d.FileName, pkg); Say("wrote " + d.FileName + " (" + pkg.Length + " bytes)"); }
        }

        void LoadTemplate(string which)
        {
            loading = true;
            if (which == "sumrange")
            {
                tEditor.Text = string.Join("\r\n", new string[] {
                    "long main(long console, long inbox)", "{",
                    "    long buf[16];", "    long lo, hi, s, i;",
                    "    syscall(3, inbox, buf, 0, 0, 0);",
                    "    lo = buf[2];", "    hi = buf[3];", "    s = 0;",
                    "    for (i = lo; i <= hi; i = i + 1)", "        s = s + i;",
                    "    syscall(2, console, 0x54584554, s, 0, 0);",
                    "    return 0;", "}" });
                tSource.Text = ""; kRecipe.Checked = false; tName.Text = "sumrange";
                kSplit.Checked = true; tLo.Text = "1"; tHi.Text = "1000000"; tPieces.Text = "8"; tBudget.Text = "15";
                SelectCombine("sum");
                loading = false; RefreshPreview(); Say("loaded sumrange: 1..1000000, 8 pieces, sum.");
            }
            else
            {
                tEditor.Text = string.Join("\r\n", new string[] {
                    "long main(long console, long inbox)", "{",
                    "    long buf[16];", "    long lo, hi, k, i;",
                    "    syscall(3, inbox, buf, 0, 0, 0);",
                    "    lo = buf[2];", "    hi = buf[3];", "    k = 0;",
                    "    for (i = lo; i <= hi; i = i + 1)", "        if ((i & 7) == 0) k = k + 1;",
                    "    syscall(2, console, 0x54584554, k, 0, 0);",
                    "    return 0;", "}" });
                tSource.Text = ""; kRecipe.Checked = false; tName.Text = "count";
                kSplit.Checked = true; tLo.Text = "1"; tHi.Text = "1000000"; tPieces.Text = "8"; tBudget.Text = "15";
                SelectCombine("sum");
                loading = false; RefreshPreview(); Say("loaded count: multiples of 8 in 1..1000000, sum.");
            }
        }

        void RefillNodeList() { nodeList.Items.Clear(); foreach (var n in nodes) nodeList.Items.Add(n); }
        void LoadSelectedNode()
        {
            var n = nodeList.SelectedItem as NodeEntry; if (n == null) return;
            tNode.Text = n.Host; tPort.Text = n.Port.ToString(); tKey.Text = n.Key;
        }
        void SaveCurrentNode()
        {
            string host = tNode.Text.Trim(); if (host.Length == 0) { Say("no host."); return; }
            NodeEntry found = null; foreach (var n in nodes) if (n.Host == host) { found = n; break; }
            if (found == null) { found = new NodeEntry(); nodes.Add(found); }
            found.Host = host; found.Name = host; int.TryParse(tPort.Text.Trim(), out found.Port); if (found.Port < 1) found.Port = 22;
            found.Key = tKey.Text.Trim();
            RefillNodeList(); Persist(); Say("saved " + host + ".");
        }
        void RemoveSelectedNode()
        {
            var n = nodeList.SelectedItem as NodeEntry; if (n == null) return;
            nodes.Remove(n); RefillNodeList(); Persist();
        }

        void LoadLast()
        {
            loading = true;
            if (last.ContainsKey("node")) tNode.Text = last["node"];
            if (last.ContainsKey("key")) tKey.Text = last["key"];
            tPort.Text = last.ContainsKey("port") ? last["port"] : "22";
            tWait.Text = last.ContainsKey("wait") ? last["wait"] : "8";
            SelectCombine(last.ContainsKey("combine") ? last["combine"] : "sum");
            loading = false;
        }
        void Persist()
        {
            if (tNode != null) last["node"] = tNode.Text.Trim();
            if (tKey != null) last["key"] = tKey.Text.Trim();
            last["combine"] = combineSel;
            if (tPort != null) last["port"] = tPort.Text.Trim();
            if (tWait != null) last["wait"] = tWait.Text.Trim();
            Store.Save(nodes, last);
        }

        void ShowAbout()
        {
            MessageBox.Show(
                "EreBUS Gate " + Ver.V + "\n\n" +
                "packages a task, feeds it to a node's desk over ssh.\n" +
                "payload: EreBUS c (far-task ABI) or a recipe, <= 1 KiB.\n" +
                "a node's door must hold your ssh key.",
                "EreBUS Gate", MessageBoxButtons.OK, MessageBoxIcon.None);
        }
    }

    /* a small read-only overview of the saved nodes: online and version,
     * polled over the door. the dashboard with jobs and live log arrives
     * with the 0.9.6 control plane. */
    class ClusterForm : Form
    {
        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
        static extern int SetWindowTheme(IntPtr h, string app, string id);

        List<NodeEntry> nodes;
        bool[] online, seen; string[] ver;
        bool busy; Point dragFrom; bool dragging;
        Panel table; System.Windows.Forms.Timer timer;

        public ClusterForm(List<NodeEntry> src)
        {
            nodes = new List<NodeEntry>(src);
            online = new bool[nodes.Count]; seen = new bool[nodes.Count]; ver = new string[nodes.Count];

            this.Text = "cluster";
            this.FormBorderStyle = FormBorderStyle.None;
            this.StartPosition = FormStartPosition.CenterParent;
            this.BackColor = Look.Ground; this.ForeColor = Look.Ink; this.Font = Look.Body;
            this.ClientSize = new Size(560, Math.Max(240, 174 + nodes.Count * 26));
            this.KeyPreview = true;
            this.KeyDown += delegate(object o, KeyEventArgs e) { if (e.KeyCode == Keys.Escape) this.Close(); };
            this.Paint += delegate(object o, PaintEventArgs e)
            { using (var pen = new Pen(Look.Edge)) e.Graphics.DrawRectangle(pen, 0, 0, this.Width - 1, this.Height - 1); };

            var head = new Panel();
            head.Bounds = new Rectangle(1, 1, this.ClientSize.Width - 2, 60); head.BackColor = Look.Ground;
            head.Paint += delegate(object o, PaintEventArgs e)
            {
                var g = e.Graphics; g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
                using (var f = new Font("Consolas", 15f, FontStyle.Bold))
                using (var b = new SolidBrush(Look.Accent)) g.DrawString("cluster", f, b, 18, 16);
                using (var pen = new Pen(Look.Edge)) g.DrawLine(pen, 18, 50, head.Width - 18, 50);
            };
            head.MouseDown += delegate(object o, MouseEventArgs e) { dragging = true; dragFrom = e.Location; };
            head.MouseUp += delegate { dragging = false; };
            head.MouseMove += delegate(object o, MouseEventArgs e)
            { if (dragging) this.Location = new Point(this.Location.X + e.X - dragFrom.X, this.Location.Y + e.Y - dragFrom.Y); };
            this.Controls.Add(head);

            var x = new Label();
            x.Text = "×"; x.Font = new Font("Consolas", 13f, FontStyle.Bold); x.ForeColor = Look.Dim;
            x.AutoSize = false; x.Size = new Size(24, 24); x.TextAlign = ContentAlignment.MiddleCenter;
            x.Location = new Point(head.Width - 30, 12); x.Cursor = Cursors.Hand;
            x.MouseEnter += delegate { x.ForeColor = Look.Accent; }; x.MouseLeave += delegate { x.ForeColor = Look.Dim; };
            x.Click += delegate { this.Close(); }; head.Controls.Add(x);

            table = new Panel(); table.BackColor = Look.Ground;
            table.Location = new Point(1, 62); table.Size = new Size(this.ClientSize.Width - 2, this.ClientSize.Height - 62 - 44);
            table.Paint += PaintTable; this.Controls.Add(table);

            var refresh = MkBtn("refresh", 18, this.ClientSize.Height - 36, 100);
            refresh.Click += delegate { Refresh_(); };
            var auto = new Toggle("auto"); auto.Location = new Point(132, this.ClientSize.Height - 32);
            auto.Changed += delegate { if (auto.Checked) timer.Start(); else timer.Stop(); };
            this.Controls.Add(auto);

            timer = new System.Windows.Forms.Timer(); timer.Interval = 8000;
            timer.Tick += delegate { Refresh_(); };
            this.FormClosing += delegate { timer.Stop(); };

            Refresh_();
        }

        Button MkBtn(string text, int x, int y, int w)
        {
            var b = new Button();
            b.Text = text; b.Font = Look.Mono; b.FlatStyle = FlatStyle.Flat;
            b.FlatAppearance.BorderColor = Look.Edge; b.FlatAppearance.BorderSize = 1; b.BackColor = Look.Ground;
            b.ForeColor = Look.Dim; b.FlatAppearance.MouseOverBackColor = Color.FromArgb(0x2C, 0x23, 0x1B);
            b.Location = new Point(x, y); b.Size = new Size(w, 26); b.Cursor = Cursors.Hand; b.UseCompatibleTextRendering = true;
            this.Controls.Add(b); return b;
        }

        void PaintTable(object o, PaintEventArgs e)
        {
            var g = e.Graphics; g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
            int x0 = 17, y = 6;
            using (var b = new SolidBrush(Look.Accent))
            {
                g.DrawString(Look.Spaced("node"), Look.Small, b, x0, y);
                g.DrawString(Look.Spaced("state"), Look.Small, b, x0 + 300, y);
                g.DrawString(Look.Spaced("version"), Look.Small, b, x0 + 392, y);
            }
            using (var pen = new Pen(Look.Edge)) g.DrawLine(pen, x0, y + 18, table.Width - 18, y + 18);
            y += 26;
            if (nodes.Count == 0)
            {
                using (var b = new SolidBrush(Look.Dim)) g.DrawString("no saved nodes.  save one in the main window.", Look.Mono, b, x0, y);
                return;
            }
            for (int i = 0; i < nodes.Count; i++)
            {
                using (var b = new SolidBrush(Look.Ink)) g.DrawString(nodes[i].ToString(), Look.Mono, b, x0, y);
                string state; Color sc;
                if (!seen[i]) { state = busy ? "..." : "-"; sc = Look.Dim; }
                else if (online[i]) { state = "online"; sc = Look.Accent; }
                else { state = "offline"; sc = Look.WarnC; }
                using (var b = new SolidBrush(sc)) g.DrawString(state, Look.Mono, b, x0 + 300, y);
                using (var b = new SolidBrush(Look.Dim)) g.DrawString(ver[i] ?? "-", Look.Mono, b, x0 + 392, y);
                y += 24;
            }
        }

        void Refresh_()
        {
            if (busy) return;
            if (nodes.Count == 0) { table.Invalidate(); return; }
            busy = true; table.Invalidate();
            var th = new Thread(delegate()
            {
                for (int i = 0; i < nodes.Count; i++)
                {
                    var n = nodes[i];
                    var s = new Spec(); s.Node = n.Host; s.Port = n.Port < 1 ? 22 : n.Port; s.Key = n.Key;
                    var lines = new List<string>();
                    int rc = Core.Probe(s, delegate(string l) { lock (lines) lines.Add(l); });
                    online[i] = rc == 0; ver[i] = Core.VersionOf(lines); seen[i] = true;
                    try { table.BeginInvoke((MethodInvoker)delegate { table.Invalidate(); }); } catch { }
                }
                busy = false;
                try { table.BeginInvoke((MethodInvoker)delegate { table.Invalidate(); }); } catch { }
            });
            th.IsBackground = true; th.Start();
        }
    }

    static class Json
    {
        public static string Write(object o) { var sb = new StringBuilder(); Emit(sb, o); return sb.ToString(); }

        static void Emit(StringBuilder sb, object o)
        {
            if (o == null) { sb.Append("null"); return; }
            if (o is string) { Str(sb, (string)o); return; }
            if (o is bool) { sb.Append(((bool)o) ? "true" : "false"); return; }
            if (o is int || o is long) { sb.Append(o.ToString()); return; }
            if (o is double) { sb.Append(((double)o).ToString("R", System.Globalization.CultureInfo.InvariantCulture)); return; }
            var dict = o as System.Collections.IDictionary;
            if (dict != null)
            {
                sb.Append('{'); bool first = true;
                foreach (System.Collections.DictionaryEntry e in dict)
                { if (!first) sb.Append(','); first = false; Str(sb, Convert.ToString(e.Key)); sb.Append(':'); Emit(sb, e.Value); }
                sb.Append('}'); return;
            }
            var list = o as System.Collections.IEnumerable;
            if (list != null)
            {
                sb.Append('['); bool first = true;
                foreach (var e in list) { if (!first) sb.Append(','); first = false; Emit(sb, e); }
                sb.Append(']'); return;
            }
            Str(sb, o.ToString());
        }

        static void Str(StringBuilder sb, string s)
        {
            sb.Append('"');
            for (int i = 0; i < s.Length; i++)
            {
                char c = s[i];
                if (c == '"') sb.Append("\\\"");
                else if (c == '\\') sb.Append("\\\\");
                else if (c == '\n') sb.Append("\\n");
                else if (c == '\r') sb.Append("\\r");
                else if (c == '\t') sb.Append("\\t");
                else if (c < 0x20) sb.Append("\\u").Append(((int)c).ToString("x4"));
                else sb.Append(c);
            }
            sb.Append('"');
        }
    }

    /* The control channel client: opens ssh, switches the session to the
     * node's binary framed protocol with "control", and speaks frames.
     * A frame is a 4-byte little-endian length and then that many bytes:
     * u8 kind, u32 id, u16 name length, the name, the body. Requests get
     * a rising id; the matching response carries it back. */
    class Ctl
    {
        public class Frm { public byte Kind; public uint Id; public string Name; public byte[] Body; }

        Process p; Stream sin, sout; uint nextId = 1;
        byte[] rbuf = new byte[0]; int rpos = 0;
        public string NodeVersion;

        public static Ctl Open(Spec s, out string err)
        {
            err = null;
            var psi = new ProcessStartInfo();
            psi.FileName = "ssh.exe";
            var a = new StringBuilder();
            a.Append("-T ");
            if (s.Port > 0 && s.Port != 22) a.Append("-p ").Append(s.Port).Append(' ');
            if (!string.IsNullOrEmpty(s.Key)) a.Append("-i \"").Append(s.Key).Append("\" ");
            a.Append("-o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 ");
            a.Append(s.Node);
            psi.Arguments = a.ToString();
            psi.UseShellExecute = false; psi.RedirectStandardInput = true;
            psi.RedirectStandardOutput = true; psi.RedirectStandardError = true; psi.CreateNoWindow = true;

            var c = new Ctl();
            try { c.p = Process.Start(psi); }
            catch (Exception e) { err = "ssh could not start: " + e.Message; return null; }
            c.sin = c.p.StandardInput.BaseStream; c.sout = c.p.StandardOutput.BaseStream;
            try
            {
                byte[] cmd = Encoding.ASCII.GetBytes("control\n");
                c.sin.Write(cmd, 0, cmd.Length); c.sin.Flush();
                if (!c.SkipPast(Encoding.ASCII.GetBytes("lists the words.\n"))) { err = "no control channel (node before 0.9.6?)"; c.Close(); return null; }
                Frm f = c.ReadFrame();
                if (f == null || f.Name != "hello") { err = "control handshake failed"; c.Close(); return null; }
                c.NodeVersion = KV(f.Body, "node");
            }
            catch (Exception e) { err = "control: " + e.Message; c.Close(); return null; }
            return c;
        }

        public Frm Req(string name, byte[] body)
        {
            try
            {
                uint id = nextId++;
                byte[] fr = FrameOf(1, id, name, body);
                sin.Write(fr, 0, fr.Length); sin.Flush();
                for (int guard = 0; guard < 100000; guard++)
                {
                    Frm f = ReadFrame();
                    if (f == null) return null;
                    if ((f.Kind == 2 || f.Kind == 4) && f.Id == id) return f;   /* skip events */
                }
            }
            catch { }
            return null;
        }

        public void Close()
        {
            try { if (sin != null) sin.Close(); } catch { }
            try { if (p != null && !p.WaitForExit(3000)) p.Kill(); } catch { }
        }

        public Frm ReadFrame()
        {
            byte[] lenb = ReadExact(4); if (lenb == null) return null;
            uint total = (uint)(lenb[0] | (lenb[1] << 8) | (lenb[2] << 16) | (lenb[3] << 24));
            if (total < 7 || total > 200000) return null;
            byte[] f = ReadExact((int)total); if (f == null) return null;
            var fr = new Frm();
            fr.Kind = f[0];
            fr.Id = (uint)(f[1] | (f[2] << 8) | (f[3] << 16) | (f[4] << 24));
            int nlen = f[5] | (f[6] << 8);
            if (7 + nlen > f.Length) return null;
            fr.Name = Encoding.ASCII.GetString(f, 7, nlen);
            int blen = (int)total - 7 - nlen;
            fr.Body = new byte[blen]; Array.Copy(f, 7 + nlen, fr.Body, 0, blen);
            return fr;
        }

        static byte[] FrameOf(byte kind, uint id, string name, byte[] body)
        {
            byte[] nm = Encoding.ASCII.GetBytes(name);
            int blen = body != null ? body.Length : 0;
            int total = 1 + 4 + 2 + nm.Length + blen;
            var o = new byte[4 + total]; int k = 0;
            o[k++] = (byte)total; o[k++] = (byte)(total >> 8); o[k++] = (byte)(total >> 16); o[k++] = (byte)(total >> 24);
            o[k++] = kind;
            o[k++] = (byte)id; o[k++] = (byte)(id >> 8); o[k++] = (byte)(id >> 16); o[k++] = (byte)(id >> 24);
            o[k++] = (byte)nm.Length; o[k++] = (byte)(nm.Length >> 8);
            Array.Copy(nm, 0, o, k, nm.Length); k += nm.Length;
            if (blen > 0) Array.Copy(body, 0, o, k, blen);
            return o;
        }

        bool Fill()
        {
            var tmp = new byte[8192];
            int r = sout.Read(tmp, 0, tmp.Length);
            if (r <= 0) return false;
            int rem = rbuf.Length - rpos;
            var nb = new byte[rem + r];
            Array.Copy(rbuf, rpos, nb, 0, rem);
            Array.Copy(tmp, 0, nb, rem, r);
            rbuf = nb; rpos = 0;
            return true;
        }

        byte[] ReadExact(int n)
        {
            while (rbuf.Length - rpos < n) { if (!Fill()) return null; }
            var o = new byte[n]; Array.Copy(rbuf, rpos, o, 0, n); rpos += n; return o;
        }

        bool SkipPast(byte[] marker)
        {
            while (true)
            {
                for (int i = rpos; i + marker.Length <= rbuf.Length; i++)
                {
                    bool ok = true;
                    for (int j = 0; j < marker.Length; j++) if (rbuf[i + j] != marker[j]) { ok = false; break; }
                    if (ok) { rpos = i + marker.Length; return true; }
                }
                if (!Fill()) return false;
            }
        }

        public static string KV(byte[] body, string key)
        {
            if (body == null) return null;
            foreach (var line in Encoding.UTF8.GetString(body).Split('\n'))
            {
                int eq = line.IndexOf('=');
                if (eq > 0 && line.Substring(0, eq) == key) return line.Substring(eq + 1);
            }
            return null;
        }
    }

    static class Program
    {
        [DllImport("kernel32.dll")]
        static extern bool AttachConsole(int pid);

        [STAThread]
        static int Main(string[] args)
        {
            if (args.Length > 0) AttachConsole(-1);
            if (args.Length > 0 && args[0] == "api") return Api(args);
            if (Has(args, "--version")) { Console.WriteLine("EreBUS Gate " + Ver.V); return 0; }
            if (Has(args, "--shot-cluster")) return ShotCluster(Arg(args, "--shot-cluster"));
            if (Has(args, "--shot")) return Shot(Arg(args, "--shot"));
            if (Has(args, "--headless")) return Headless(args);
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new GateForm());
            return 0;
        }

        static bool Has(string[] a, string k) { foreach (var x in a) if (x == k) return true; return false; }
        static string Arg(string[] a, string k) { for (int i = 0; i < a.Length - 1; i++) if (a[i] == k) return a[i + 1]; return null; }

        static int Shot(string path)
        {
            if (string.IsNullOrEmpty(path)) { Console.Error.WriteLine("--shot needs a file"); return 2; }
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            var f = new GateForm();
            f.StartPosition = FormStartPosition.Manual; f.Location = new Point(-4000, -4000);
            f.Show(); Application.DoEvents();
            using (var bmp = new Bitmap(f.Width, f.Height))
            { f.DrawToBitmap(bmp, new Rectangle(0, 0, f.Width, f.Height)); bmp.Save(path, ImageFormat.Png); }
            f.Close();
            Console.WriteLine("wrote " + path);
            return 0;
        }

        static int ShotCluster(string path)
        {
            if (string.IsNullOrEmpty(path)) { Console.Error.WriteLine("--shot-cluster needs a file"); return 2; }
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            var demo = new List<NodeEntry>();
            demo.Add(new NodeEntry() { Name = "alpha", Host = "someone@alpha", Port = 22 });
            demo.Add(new NodeEntry() { Name = "beta",  Host = "someone@beta",  Port = 2222 });
            demo.Add(new NodeEntry() { Name = "gamma", Host = "someone@gamma", Port = 22 });
            var f = new ClusterForm(demo);
            f.StartPosition = FormStartPosition.Manual; f.Location = new Point(-4000, -4000);
            f.Show(); Application.DoEvents(); System.Threading.Thread.Sleep(150); Application.DoEvents();
            using (var bmp = new Bitmap(f.Width, f.Height))
            { f.DrawToBitmap(bmp, new Rectangle(0, 0, f.Width, f.Height)); bmp.Save(path, ImageFormat.Png); }
            f.Close();
            Console.WriteLine("wrote " + path);
            return 0;
        }

        static int Headless(string[] a)
        {
            var s = new Spec();
            s.Source = Arg(a, "--source"); s.Node = Arg(a, "--node"); s.Key = Arg(a, "--key");
            s.Name = Arg(a, "--name"); s.InputName = Arg(a, "--input"); s.Combine = Arg(a, "--combine");
            s.Recipe = Has(a, "--recipe");
            int v;
            if (int.TryParse(Arg(a, "--port") ?? "", out v)) s.Port = v;
            if (int.TryParse(Arg(a, "--across") ?? "", out v)) s.Across = v;
            if (int.TryParse(Arg(a, "--pieces") ?? "", out v)) s.Pieces = v;
            if (int.TryParse(Arg(a, "--budget") ?? "", out v)) s.Budget = v;
            s.Wait = int.TryParse(Arg(a, "--wait") ?? "", out v) ? v : 8;
            long lo, hi;
            if (long.TryParse(Arg(a, "--lo") ?? "", out lo) && long.TryParse(Arg(a, "--hi") ?? "", out hi))
            { s.HasSplit = true; s.SplitLo = lo; s.SplitHi = hi; }
            if (string.IsNullOrEmpty(s.Source)) { Console.Error.WriteLine("--headless needs --source"); return 2; }
            string err; byte[] pkg = Core.BuildPackage(s, out err);
            if (pkg == null) { Console.Error.WriteLine(err); return 1; }
            string outp = Arg(a, "--out");
            if (!string.IsNullOrEmpty(outp)) { File.WriteAllBytes(outp, pkg); Console.WriteLine("wrote " + outp + " (" + pkg.Length + " bytes)"); return 0; }
            if (string.IsNullOrEmpty(s.Node)) { Console.Out.Write(new UTF8Encoding(false).GetString(pkg)); return 0; }
            return Core.Feed(s, pkg, Console.WriteLine);
        }

        /* ---- machine interface: "api <verb>", json on stdout ---- */

        static OrderedDictionary Obj() { return new OrderedDictionary(); }

        static int Emit(object o)
        {
            Console.Out.WriteLine(Json.Write(o)); Console.Out.Flush();
            var d = o as OrderedDictionary;
            return (d != null && d.Contains("ok") && (d["ok"] is bool) && !(bool)d["ok"]) ? 1 : 0;
        }

        static void EmitLine(object o) { Console.Out.WriteLine(Json.Write(o)); Console.Out.Flush(); }

        static int Fail(string msg)
        {
            var o = Obj(); o["ok"] = false; o["error"] = msg;
            Console.Out.WriteLine(Json.Write(o)); Console.Out.Flush();
            return 1;
        }

        static long Now() { return (long)(DateTime.UtcNow - new DateTime(1970, 1, 1)).TotalSeconds; }
        static Action<string> Sink(List<string> into) { return delegate(string l) { lock (into) into.Add(l); }; }

        static int Api(string[] a)
        {
            string verb = a.Length > 1 ? a[1] : "";
            switch (verb)
            {
                case "version": return ApiVersion();
                case "schema":  return ApiSchema();
                case "nodes":   return ApiNodes(a);
                case "status":  return ApiStatus(a);
                case "jobs":    return ApiJobs(a);
                case "submit":  return ApiSubmit(a);
                case "result":  return ApiResult(a);
                case "scan":    return ApiScan(a);
                case "door":    return ApiDoor(a);
                case "watch":   return ApiWatch(a);
                case "log":     return ApiLog(a);
                case "peers":   return ApiPeers(a);
                case "cluster": return ApiCluster(a);
                default:        return Fail("unknown verb '" + verb + "'; try: api schema");
            }
        }

        static Spec NodeSpec(string[] a)
        {
            var s = new Spec();
            string node = Arg(a, "--node"); if (string.IsNullOrEmpty(node)) node = Arg(a, "--host");
            string key = Arg(a, "--key");
            int port = 0; int.TryParse(Arg(a, "--port") ?? "", out port);
            var nodes = new List<NodeEntry>(); var last = new Dictionary<string, string>();
            Store.Load(nodes, last);
            NodeEntry m = null;
            if (!string.IsNullOrEmpty(node)) foreach (var n in nodes) if (n.Name == node || n.Host == node) { m = n; break; }
            if (m != null) { s.Node = m.Host; s.Port = m.Port; s.Key = m.Key; }
            else s.Node = node;
            if (port >= 1) s.Port = port;
            if (!string.IsNullOrEmpty(key)) s.Key = key;
            if (s.Port < 1) s.Port = 22;
            return s;
        }

        static void ApplySplit(Spec s, string[] a)
        {
            long lo, hi;
            string sp = Arg(a, "--split");
            if (!string.IsNullOrEmpty(sp))
            {
                string[] parts = sp.Replace("..", " ").Split(new char[] { ' ', ',' }, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length == 2 && long.TryParse(parts[0], out lo) && long.TryParse(parts[1], out hi))
                { s.HasSplit = true; s.SplitLo = lo; s.SplitHi = hi; return; }
            }
            if (long.TryParse(Arg(a, "--lo") ?? "", out lo) && long.TryParse(Arg(a, "--hi") ?? "", out hi))
            { s.HasSplit = true; s.SplitLo = lo; s.SplitHi = hi; }
        }

        static string FindVersion(List<string> lines) { return Core.VersionOf(lines); }

        static object FindJob(List<string> lines)
        {
            foreach (var l in lines)
            {
                int idx = l.IndexOf("job ");
                if (idx < 0) continue;
                if (!(l.Contains("pipe:") || l.Contains("queued") || l.Contains("created"))) continue;
                int p = idx + 4, e = p;
                while (e < l.Length && l[e] >= '0' && l[e] <= '9') e++;
                long n; if (e > p && long.TryParse(l.Substring(p, e - p), out n)) return n;
            }
            return null;
        }

        static string FindResult(List<string> lines)
        {
            for (int i = lines.Count - 1; i >= 0; i--)
            {
                string t = lines[i].TrimStart();
                if (t.StartsWith("= ")) return t.Substring(2).Trim();
            }
            return null;
        }

        static bool FindAccepted(List<string> lines)
        {
            foreach (var l in lines)
                if (l.Contains("submitted to the desk") || l.Contains("created here")) return true;
            return false;
        }

        static object ToLong(string s) { long v; return long.TryParse(s, out v) ? (object)v : null; }

        /* One control body line, "k=v k=v ...", into the object; a
         * trailing "text=" takes the rest of the line as one value. */
        static void KvLine(OrderedDictionary o, string line)
        {
            string head = line, text = null;
            int ti = line.IndexOf(" text=");
            if (ti >= 0) { text = line.Substring(ti + 6); head = line.Substring(0, ti); }
            else if (line.StartsWith("text=")) { text = line.Substring(5); head = ""; }
            foreach (var tok in head.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries))
            {
                int eq = tok.IndexOf('=');
                if (eq > 0) o[tok.Substring(0, eq)] = tok.Substring(eq + 1);
            }
            if (text != null) o["text"] = text;
        }

        static int ApiVersion()
        {
            var o = Obj(); o["ok"] = true; o["name"] = "EreBUS Gate"; o["version"] = Ver.V; o["schema"] = 2;
            return Emit(o);
        }

        static object V(string verb, string args, string desc) { var o = Obj(); o["verb"] = verb; o["args"] = args; o["desc"] = desc; return o; }

        static int ApiSchema()
        {
            var verbs = new List<object>();
            verbs.Add(V("version", "", "tool name, version, schema number"));
            verbs.Add(V("schema", "", "this list of verbs"));
            verbs.Add(V("nodes", "[--probe]", "saved nodes; --probe adds online and version"));
            verbs.Add(V("status", "--node [--port --key]", "one node over the control channel: version, uptime_s, jobs (falls back to a line probe on an older node)"));
            verbs.Add(V("jobs", "--node", "the desk's jobs in flight: no, name, state, pieces, parts, done, quorum, combine"));
            verbs.Add(V("submit", "--source --node [--recipe, --split lo..hi | --lo --hi, --pieces --across --combine --budget --input --name --out --wait]", "package a task and hand it to the desk over the control channel; returns a handle, and with --wait the folded result"));
            verbs.Add(V("result", "--node [--name --wait]", "read a named object back on the line channel; returns its folded result"));
            verbs.Add(V("scan", "--node", "scan the mesh from a node; returns the raw found/nodes text"));
            verbs.Add(V("door", "--grant key.pub", "the 'write door | ...' line that authorises an ssh key on a node"));
            verbs.Add(V("watch", "--node [--for s]", "subscribe to a node and stream one json event per far-work transition (queued/done/failed) as it is pushed"));
            verbs.Add(V("log", "--node [--lines N]", "the tail of the node's journal (default 20 lines)"));
            verbs.Add(V("peers", "--node", "the cluster as the node has heard it: ip, name, version, jobs, works, free_mib, seen_s, up_min"));
            verbs.Add(V("cluster", "--node", "this node plus every peer with its live job count, aggregated by the node (a scan is poked, then read)"));
            var o = Obj(); o["ok"] = true; o["name"] = "EreBUS Gate"; o["version"] = Ver.V; o["schema"] = 2; o["transport"] = "control over ssh"; o["verbs"] = verbs;
            return Emit(o);
        }

        static int ApiNodes(string[] a)
        {
            var nodes = new List<NodeEntry>(); var last = new Dictionary<string, string>();
            Store.Load(nodes, last);
            bool probe = Has(a, "--probe");
            var arr = new List<object>();
            foreach (var n in nodes)
            {
                var o = Obj(); o["name"] = n.Name; o["host"] = n.Host; o["port"] = n.Port; o["key"] = n.Key;
                if (probe)
                {
                    var s = new Spec(); s.Node = n.Host; s.Port = n.Port < 1 ? 22 : n.Port; s.Key = n.Key;
                    var lines = new List<string>(); int rc = Core.Probe(s, Sink(lines));
                    o["online"] = rc == 0; o["version"] = FindVersion(lines);
                }
                arr.Add(o);
            }
            var top = Obj(); top["ok"] = true; top["count"] = nodes.Count; top["nodes"] = arr;
            return Emit(top);
        }

        static int ApiStatus(string[] a)
        {
            var s = NodeSpec(a);
            if (string.IsNullOrEmpty(s.Node)) return Fail("status needs --node");
            string cerr; var c = Ctl.Open(s, out cerr);
            if (c != null)
            {
                var f = c.Req("status", null); c.Close();
                if (f != null && f.Kind == 2)
                {
                    var o = Obj(); o["ok"] = true; o["node"] = s.Node; o["online"] = true; o["via"] = "control";
                    o["version"] = Ctl.KV(f.Body, "version");
                    o["uptime_s"] = ToLong(Ctl.KV(f.Body, "uptime_s"));
                    o["jobs"] = ToLong(Ctl.KV(f.Body, "jobs"));
                    return Emit(o);
                }
                var e = Obj(); e["ok"] = false; e["node"] = s.Node; e["error"] = f != null ? Encoding.UTF8.GetString(f.Body) : "no response"; return Emit(e);
            }
            /* older node or no control channel: the line probe */
            var lines = new List<string>(); int rc = Core.Probe(s, Sink(lines));
            var o2 = Obj(); o2["ok"] = true; o2["node"] = s.Node; o2["online"] = rc == 0; o2["via"] = "line";
            o2["version"] = FindVersion(lines); o2["note"] = cerr; o2["raw"] = lines;
            return Emit(o2);
        }

        static int ApiJobs(string[] a)
        {
            var s = NodeSpec(a);
            if (string.IsNullOrEmpty(s.Node)) return Fail("jobs needs --node");
            string cerr; var c = Ctl.Open(s, out cerr);
            if (c == null) return Fail(cerr != null ? cerr : "control channel not available");
            var f = c.Req("jobs", null); c.Close();
            if (f == null || f.Kind != 2) return Fail(f != null ? Encoding.UTF8.GetString(f.Body) : "no response");
            var arr = new List<object>(); long count = 0;
            foreach (var line in Encoding.UTF8.GetString(f.Body).Split('\n'))
            {
                if (line.Length == 0) continue;
                if (line.StartsWith("count=")) { long.TryParse(line.Substring(6), out count); continue; }
                var jo = Obj(); KvLine(jo, line); arr.Add(jo);
            }
            var o = Obj(); o["ok"] = true; o["node"] = s.Node; o["via"] = "control"; o["count"] = count; o["jobs"] = arr;
            return Emit(o);
        }

        static int ApiPeers(string[] a)
        {
            var s = NodeSpec(a);
            if (string.IsNullOrEmpty(s.Node)) return Fail("peers needs --node");
            string cerr; var c = Ctl.Open(s, out cerr);
            if (c == null) return Fail(cerr != null ? cerr : "control channel not available");
            var f = c.Req("peers", null); c.Close();
            if (f == null || f.Kind != 2) return Fail(f != null ? Encoding.UTF8.GetString(f.Body) : "no response");
            var arr = new List<object>(); long count = 0;
            foreach (var line in Encoding.UTF8.GetString(f.Body).Split('\n'))
            {
                if (line.Length == 0) continue;
                if (line.StartsWith("count=")) { long.TryParse(line.Substring(6), out count); continue; }
                var po = Obj(); KvLine(po, line); arr.Add(po);
            }
            var o = Obj(); o["ok"] = true; o["node"] = s.Node; o["via"] = "control"; o["count"] = count; o["peers"] = arr;
            return Emit(o);
        }

        static int ApiCluster(string[] a)
        {
            var s = NodeSpec(a);
            if (string.IsNullOrEmpty(s.Node)) return Fail("cluster needs --node");
            string cerr; var c = Ctl.Open(s, out cerr);
            if (c == null) return Fail(cerr != null ? cerr : "control channel not available");
            c.Req("cluster", null);      /* the first call pokes a scan */
            Thread.Sleep(4200);          /* let the peers answer */
            var f = c.Req("cluster", null); c.Close();
            if (f == null || f.Kind != 2) return Fail(f != null ? Encoding.UTF8.GetString(f.Body) : "no response");
            var peers = new List<object>(); OrderedDictionary self = null; long np = 0;
            foreach (var line in Encoding.UTF8.GetString(f.Body).Split('\n'))
            {
                if (line.Length == 0) continue;
                if (line.StartsWith("peers=")) { long.TryParse(line.Substring(6), out np); continue; }
                if (line.StartsWith("self ")) { self = Obj(); KvLine(self, line.Substring(5)); continue; }
                if (line.StartsWith("peer ")) { var po = Obj(); KvLine(po, line.Substring(5)); peers.Add(po); continue; }
            }
            var o = Obj(); o["ok"] = true; o["node"] = s.Node; o["via"] = "control"; o["peers_count"] = np; o["self"] = self; o["peers"] = peers;
            return Emit(o);
        }

        static int ApiLog(string[] a)
        {
            var s = NodeSpec(a);
            if (string.IsNullOrEmpty(s.Node)) return Fail("log needs --node");
            int n; byte[] body = int.TryParse(Arg(a, "--lines") ?? "", out n) && n > 0 ? Encoding.ASCII.GetBytes(n.ToString()) : null;
            string cerr; var c = Ctl.Open(s, out cerr);
            if (c == null) return Fail(cerr != null ? cerr : "control channel not available");
            var f = c.Req("log", body); c.Close();
            if (f == null || f.Kind != 2) return Fail(f != null ? Encoding.UTF8.GetString(f.Body) : "no response");
            var arr = new List<object>();
            foreach (var line in Encoding.UTF8.GetString(f.Body).Split('\n')) if (line.Length > 0) arr.Add(line);
            var o = Obj(); o["ok"] = true; o["node"] = s.Node; o["via"] = "control"; o["lines"] = arr;
            return Emit(o);
        }

        static int ApiSubmit(string[] a)
        {
            var s = NodeSpec(a);
            s.Source = Arg(a, "--source"); s.Name = Arg(a, "--name"); s.InputName = Arg(a, "--input"); s.Combine = Arg(a, "--combine");
            s.Recipe = Has(a, "--recipe");
            int v;
            if (int.TryParse(Arg(a, "--across") ?? "", out v)) s.Across = v;
            if (int.TryParse(Arg(a, "--pieces") ?? "", out v)) s.Pieces = v;
            if (int.TryParse(Arg(a, "--budget") ?? "", out v)) s.Budget = v;
            s.Wait = int.TryParse(Arg(a, "--wait") ?? "", out v) ? v : 0;
            ApplySplit(s, a);
            if (string.IsNullOrEmpty(s.Source)) return Fail("submit needs --source");
            string err; byte[] pkg = Core.BuildPackage(s, out err);
            if (pkg == null) return Fail(err);
            if (pkg.Length > Core.WireMax) return Fail("payload " + pkg.Length + " B > " + Core.WireMax + " B");
            string outp = Arg(a, "--out");
            if (!string.IsNullOrEmpty(outp))
            {
                try { File.WriteAllBytes(outp, pkg); } catch (Exception e) { return Fail(e.Message); }
                var w = Obj(); w["ok"] = true; w["wrote"] = outp; w["bytes"] = pkg.Length; return Emit(w);
            }
            if (string.IsNullOrEmpty(s.Node)) return Fail("submit needs --node (or --out to write the package)");

            string cerr; var c = Ctl.Open(s, out cerr);
            if (c != null)
            {
                var fs = c.Req("submit", pkg);
                if (fs == null || fs.Kind != 2) { string em = fs != null ? Encoding.UTF8.GetString(fs.Body) : "no response"; c.Close(); return Fail(em); }
                string handle = Ctl.KV(fs.Body, "handle");
                var o = Obj(); o["ok"] = true; o["node"] = s.Node; o["via"] = "control"; o["bytes"] = pkg.Length; o["handle"] = ToLong(handle);
                if (s.Wait > 0 && handle != null)
                {
                    DateTime dl = DateTime.UtcNow.AddSeconds(s.Wait);
                    string state = "pending", result = null;
                    byte[] hb = Encoding.ASCII.GetBytes(handle);
                    while (DateTime.UtcNow < dl)
                    {
                        Thread.Sleep(400);
                        var fr = c.Req("result", hb);
                        if (fr != null && fr.Kind == 2)
                        {
                            state = Ctl.KV(fr.Body, "state") ?? "pending";
                            result = Ctl.KV(fr.Body, "result");
                            if (state == "done") break;
                        }
                    }
                    o["state"] = state; o["result"] = result;
                }
                c.Close();
                return Emit(o);
            }
            /* older node or no control channel: the line feed */
            var lines = new List<string>(); int rc = Core.Feed(s, pkg, Sink(lines));
            var o2 = Obj(); o2["ok"] = rc == 0; o2["node"] = s.Node; o2["via"] = "line"; o2["bytes"] = pkg.Length; o2["name"] = s.Name;
            o2["accepted"] = FindAccepted(lines); o2["result"] = FindResult(lines); o2["note"] = cerr; o2["raw"] = lines;
            return Emit(o2);
        }

        static int ApiResult(string[] a)
        {
            var s = NodeSpec(a);
            if (string.IsNullOrEmpty(s.Node)) return Fail("result needs --node");
            string name = Arg(a, "--name"); if (string.IsNullOrEmpty(name)) name = "job";
            int v; s.Wait = int.TryParse(Arg(a, "--wait") ?? "", out v) ? v : 0;
            var lines = new List<string>(); int rc = Core.Read(s, name, Sink(lines));
            var o = Obj(); o["ok"] = rc == 0; o["node"] = s.Node; o["name"] = name; o["result"] = FindResult(lines); o["exit"] = rc; o["raw"] = lines;
            return Emit(o);
        }

        static int ApiScan(string[] a)
        {
            var s = NodeSpec(a);
            if (string.IsNullOrEmpty(s.Node)) return Fail("scan needs --node");
            var lines = new List<string>(); int rc = Core.Scan(s, Sink(lines));
            var o = Obj(); o["ok"] = rc == 0; o["node"] = s.Node; o["exit"] = rc; o["raw"] = lines;
            return Emit(o);
        }

        static int ApiDoor(string[] a)
        {
            string grant = Arg(a, "--grant");
            if (string.IsNullOrEmpty(grant)) return Fail("door needs --grant <ssh public key file>");
            string pub;
            try { pub = File.ReadAllText(grant).Trim(); } catch (Exception e) { return Fail("cannot read key: " + e.Message); }
            string[] parts = pub.Split(new char[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length < 2) return Fail("not an ssh public key");
            var o = Obj(); o["ok"] = true; o["type"] = parts[0]; o["line"] = "write door | " + parts[0] + " " + parts[1];
            return Emit(o);
        }

        static int ApiWatch(string[] a)
        {
            string nodesArg = Arg(a, "--node"); if (string.IsNullOrEmpty(nodesArg)) nodesArg = Arg(a, "--host");
            if (string.IsNullOrEmpty(nodesArg)) return Fail("watch needs --node");
            string first = nodesArg.Split(new char[] { ',' }, StringSplitOptions.RemoveEmptyEntries)[0].Trim();
            int forS; bool bounded = int.TryParse(Arg(a, "--for") ?? "", out forS);
            var s = NodeSpec(new string[] { "--node", first, "--key", Arg(a, "--key") ?? "", "--port", Arg(a, "--port") ?? "" });

            string cerr; var c = Ctl.Open(s, out cerr);
            if (c == null) { var e = Obj(); e["event"] = "error"; e["error"] = cerr; e["t"] = Now(); EmitLine(e); return 1; }

            var subf = c.Req("subscribe", null);
            if (subf == null || subf.Kind != 2) { var e = Obj(); e["event"] = "error"; e["error"] = "subscribe failed"; e["t"] = Now(); EmitLine(e); c.Close(); return 1; }

            var start = Obj(); start["event"] = "start"; start["mode"] = "push"; start["node"] = first; start["version"] = c.NodeVersion; start["t"] = Now();
            EmitLine(start);

            /* the node pushes event frames as they occur; read them off the wire */
            var rt = new Thread(delegate()
            {
                try
                {
                    while (true)
                    {
                        var f = c.ReadFrame();
                        if (f == null) break;
                        if (f.Kind == 3)
                        {
                            var o = Obj(); o["event"] = "job"; o["node"] = first; KvLine(o, Encoding.UTF8.GetString(f.Body)); o["t"] = Now();
                            EmitLine(o);
                        }
                    }
                }
                catch { }
            });
            rt.IsBackground = true; rt.Start();

            bool stop = false;
            var th = new Thread(delegate()
            {
                try { string l; while ((l = Console.In.ReadLine()) != null) if (l.Trim() == "stop") { stop = true; break; } }
                catch { }
            });
            th.IsBackground = true; th.Start();

            DateTime deadline = bounded ? DateTime.UtcNow.AddSeconds(forS) : DateTime.MaxValue;
            while (!stop && DateTime.UtcNow < deadline && !rt.Join(0)) Thread.Sleep(100);

            c.Close();
            var end = Obj(); end["event"] = "end"; end["t"] = Now(); EmitLine(end);
            return 0;
        }
    }
}
