/*
 * EreBUS Gate -- a Windows front-end that packages a task and feeds it into
 * an EreBUS node over ssh, where the desk distributes it among the machines.
 *
 * It builds a .ebtask package (a "key | value" manifest, a "--" line, then the
 * payload) from the fields and pipes it into the node's terminal through the
 * built-in OpenSSH client: "receive <n> bytes as job", the package, "submit
 * job", then reads the task back so the folded result shows.
 *
 * No SDK: build with the .NET Framework compiler.
 *   tools\gate\build.cmd            -> build\gate\EreBUS-Gate.exe
 * No arguments opens the window; --headless drives the same core from a
 * command line; --shot <file.png> renders the window to an image.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Text;
using System.Threading;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("EreBUS Gate")]
[assembly: System.Reflection.AssemblyVersion("0.1.0.0")]
[assembly: System.Reflection.AssemblyFileVersion("0.1.0.0")]

namespace EreBUSGate
{
    static class Ver { public const string V = "0.1"; }

    /* The look, taken from EreBUS's own shell: one warm near-black ground, a
     * cream ink, a single burnt-orange accent for what acts, thin rules;
     * spaced capitals for region labels, lowercase for the words you press. */
    static class Look
    {
        public static readonly Color Ground = Color.FromArgb(0x16, 0x13, 0x10);
        public static readonly Color Panel  = Color.FromArgb(0x1F, 0x1B, 0x16);
        public static readonly Color Field  = Color.FromArgb(0x24, 0x1F, 0x19);
        public static readonly Color Ink    = Color.FromArgb(0xDD, 0xD6, 0xC8);
        public static readonly Color Dim    = Color.FromArgb(0x8C, 0x84, 0x74);
        public static readonly Color Faint  = Color.FromArgb(0x5E, 0x57, 0x4B);
        public static readonly Color Accent = Color.FromArgb(0xC8, 0x5A, 0x28);
        public static readonly Color Warn   = Color.FromArgb(0xD8, 0x9A, 0x3A);
        public static readonly Color Edge   = Color.FromArgb(0x33, 0x2E, 0x27);
        public static Font Body  = new Font("Consolas", 10.5f, FontStyle.Regular, GraphicsUnit.Point);
        public static Font Mono  = new Font("Consolas", 9.5f,  FontStyle.Regular, GraphicsUnit.Point);
        public static Font Small = new Font("Consolas", 8.25f, FontStyle.Regular, GraphicsUnit.Point);
        public static Font Head  = new Font("Consolas", 22f,   FontStyle.Bold,    GraphicsUnit.Point);

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

    /* Where nodes and the last task are kept between runs. */
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

    /* The package and the wire feed -- no UI, so it can be tested on its own. */
    static class Core
    {
        public const int WireMax = 1024;   /* RECIPE_MAX in kernel/net/pipe.c */
        public const int SlotMax = 8;      /* PART_MAX */

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

        public static byte[] BuildPackage(Spec s, out string err)
        {
            err = null;
            byte[] payload;
            try { payload = File.ReadAllBytes(s.Source); }
            catch (Exception e) { err = "cannot read the source: " + e.Message; return null; }
            payload = NormalizeLf(payload);
            byte[] manifest = new UTF8Encoding(false).GetBytes(Manifest(s));
            var ms = new MemoryStream();
            ms.Write(manifest, 0, manifest.Length);
            ms.Write(payload, 0, payload.Length);
            if (payload.Length == 0 || payload[payload.Length - 1] != (byte)'\n') ms.WriteByte((byte)'\n');
            return ms.ToArray();
        }

        public static int Feed(Spec s, byte[] pkg, Action<string> log)
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
            psi.UseShellExecute = false;
            psi.RedirectStandardInput = true;
            psi.RedirectStandardOutput = true;
            psi.RedirectStandardError = true;
            psi.CreateNoWindow = true;

            Process p;
            try { p = Process.Start(psi); }
            catch (Exception e) { log("ssh could not start: " + e.Message); return -1; }

            p.OutputDataReceived += delegate(object o, DataReceivedEventArgs e) { if (e.Data != null) log(e.Data); };
            p.ErrorDataReceived  += delegate(object o, DataReceivedEventArgs e) { if (e.Data != null) log(e.Data); };
            p.BeginOutputReadLine();
            p.BeginErrorReadLine();

            try
            {
                var stdin = p.StandardInput.BaseStream;
                Write(stdin, "receive " + pkg.Length + " bytes as job\n");
                stdin.Write(pkg, 0, pkg.Length);
                Write(stdin, "submit job\n");
                stdin.Flush();
                if (s.Wait > 0)
                {
                    log("... waiting " + s.Wait + "s");
                    Thread.Sleep(s.Wait * 1000);
                    Write(stdin, "read job\n");
                    stdin.Flush();
                    Thread.Sleep(1500);
                }
                stdin.Close();
            }
            catch (Exception e) { log("the wire broke: " + e.Message); }

            if (!p.WaitForExit(25000)) { try { p.Kill(); } catch { } }
            return p.HasExited ? p.ExitCode : 0;
        }

        /* A quick reachability and identity check: run 'version' over the door. */
        public static int Probe(Spec s, Action<string> log)
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
            try { p = Process.Start(psi); } catch (Exception e) { log("ssh could not start: " + e.Message); return -1; }
            p.OutputDataReceived += delegate(object o, DataReceivedEventArgs e) { if (e.Data != null) log(e.Data); };
            p.ErrorDataReceived  += delegate(object o, DataReceivedEventArgs e) { if (e.Data != null) log(e.Data); };
            p.BeginOutputReadLine(); p.BeginErrorReadLine();
            try { Write(p.StandardInput.BaseStream, "version\nwhere\n"); p.StandardInput.BaseStream.Flush(); p.StandardInput.Close(); }
            catch (Exception e) { log("the wire broke: " + e.Message); }
            if (!p.WaitForExit(15000)) { try { p.Kill(); } catch { } }
            return p.HasExited ? p.ExitCode : 0;
        }

        static void Write(Stream st, string ascii) { byte[] b = Encoding.ASCII.GetBytes(ascii); st.Write(b, 0, b.Length); }
    }

    class GateForm : Form
    {
        TextBox tSource, tNode, tKey, tLo, tHi, tPieces, tAcross, tBudget, tInputName, tName, tPreview;
        NumericUpDown nPort, nWait;
        ComboBox cCombine;
        CheckBox kSplit, kRecipe;
        ListBox nodeList;
        RichTextBox log;
        Label planLabel;
        Button send;
        ToolTip tips = new ToolTip();
        List<NodeEntry> nodes = new List<NodeEntry>();
        Dictionary<string, string> last = new Dictionary<string, string>();
        Point dragFrom; bool dragging; bool loading;

        public GateForm()
        {
            Store.Load(nodes, last);
            this.Text = "EreBUS Gate";
            this.FormBorderStyle = FormBorderStyle.None;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Look.Ground;
            this.ForeColor = Look.Ink;
            this.Font = Look.Body;
            this.ClientSize = new Size(1000, 748);
            this.AllowDrop = true;
            this.KeyPreview = true;
            this.Paint += delegate(object o, PaintEventArgs e)
            {
                using (var pen = new Pen(Look.Edge))
                {
                    e.Graphics.DrawRectangle(pen, 0, 0, this.Width - 1, this.Height - 1);
                    e.Graphics.DrawLine(pen, 492, 108, 492, this.Height - 40);   /* the column rule */
                }
            };
            this.DragEnter += delegate(object o, DragEventArgs e) { if (e.Data.GetDataPresent(DataFormats.FileDrop)) e.Effect = DragDropEffects.Copy; };
            this.DragDrop += delegate(object o, DragEventArgs e)
            {
                string[] f = (string[])e.Data.GetData(DataFormats.FileDrop);
                if (f != null && f.Length > 0) { tSource.Text = f[0]; if (f[0].EndsWith(".recipe", StringComparison.OrdinalIgnoreCase)) kRecipe.Checked = true; }
            };
            this.KeyDown += delegate(object o, KeyEventArgs e) { if (e.KeyCode == Keys.Escape) this.Close(); };
            this.FormClosing += delegate { Persist(); };

            BuildHeader();
            BuildLeft();
            BuildRight();
            LoadLast();
            RefreshPreview();
        }

        void BuildHeader()
        {
            var head = new Panel();
            head.Bounds = new Rectangle(1, 1, this.ClientSize.Width - 2, 100);
            head.BackColor = Look.Ground;
            head.Paint += delegate(object o, PaintEventArgs e)
            {
                var g = e.Graphics;
                g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
                using (var b = new SolidBrush(Look.Accent)) g.DrawString("E R E B U S   G A T E", Look.Head, b, 24, 20);
                using (var b = new SolidBrush(Look.Dim))
                {
                    g.DrawString("v " + Ver.V, Look.Small, b, 27, 66);
                    g.DrawString("task -> ssh -> desk", Look.Small, b, 74, 66);
                }
                using (var pen = new Pen(Look.Edge)) g.DrawLine(pen, 24, 92, head.Width - 24, 92);
            };
            head.MouseDown += delegate(object o, MouseEventArgs e) { dragging = true; dragFrom = e.Location; };
            head.MouseUp += delegate { dragging = false; };
            head.MouseMove += delegate(object o, MouseEventArgs e)
            { if (dragging) this.Location = new Point(this.Location.X + e.X - dragFrom.X, this.Location.Y + e.Y - dragFrom.Y); };
            this.Controls.Add(head);
            head.Controls.Add(Glyph("\u00D7", head.Width - 34, 16, delegate { this.Close(); }));
            head.Controls.Add(Glyph("\u2013", head.Width - 66, 16, delegate { this.WindowState = FormWindowState.Minimized; }));
            head.Controls.Add(Glyph("?", head.Width - 98, 16, delegate { ShowAbout(); }));
        }

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
            t.BorderStyle = BorderStyle.FixedSingle; t.BackColor = Look.Field; t.ForeColor = Look.Ink;
            t.Font = Look.Body; t.Location = new Point(x, y); t.Size = new Size(w, 24);
            t.TextChanged += delegate { RefreshPreview(); };
            if (tip != null) tips.SetToolTip(t, tip);
            this.Controls.Add(t);
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
            this.Controls.Add(b);
            return b;
        }

        void BuildLeft()
        {
            int x = 24, y = 116, w = 444;
            Section("the task", x, y); y += 30;

            Field("source", x, y, w);
            tSource = Text_(x, y + 15, w - 84, "c source or recipe; drag a file here");
            var browse = Btn("browse", x + w - 78, y + 14, 78, 26, false);
            browse.Click += delegate
            {
                var d = new OpenFileDialog(); d.Filter = "programs and recipes|*.c;*.recipe;*.txt|all files|*.*";
                if (d.ShowDialog() == DialogResult.OK) { tSource.Text = d.FileName; if (d.FileName.EndsWith(".recipe", StringComparison.OrdinalIgnoreCase)) kRecipe.Checked = true; }
            };
            y += 46;

            kRecipe = Check("recipe", x, y, "checked: recipe for the interpreter. unchecked: c the node compiles");
            Field("name", x + 250, y - 2, 60);
            tName = Text_(x + 250, y + 12, w - 250, "task name, for the log");
            y += 44;

            kSplit = Check("split", x, y, "divide lo..hi into pieces, one per machine");
            y += 26;
            Field("from", x, y, 60);      tLo = Text_(x, y + 15, 120, "low end");
            Field("to", x + 132, y, 40);  tHi = Text_(x + 132, y + 15, 120, "high end");
            Field("pieces", x + 264, y, 60); tPieces = Text_(x + 264, y + 15, 90, "count of pieces; blank = 4");
            y += 46;

            Field("combine", x, y, 160);
            cCombine = Combo(x, y + 15, 130, new string[] { "sum", "concat", "min", "max", "count", "first" },
                             "how the pieces fold");
            Field("across", x + 150, y, 150); tAcross = Text_(x + 150, y + 15, 80, "quorum: each piece on N machines, majority");
            Field("budget", x + 244, y, 120); tBudget = Text_(x + 244, y + 15, 80, "seconds per worker");
            Field("input", x + 340, y, 120); tInputName = Text_(x + 340, y + 15, w - 340, "petname of an object on the node");
            y += 46;

            planLabel = new Label(); planLabel.AutoSize = false; planLabel.Location = new Point(x, y);
            planLabel.Size = new Size(w, 34); planLabel.Font = Look.Small; planLabel.ForeColor = Look.Dim;
            this.Controls.Add(planLabel);
            y += 38;

            Field("package", x, y, w);
            tPreview = new TextBox();
            tPreview.Multiline = true; tPreview.ReadOnly = true; tPreview.ScrollBars = ScrollBars.Vertical;
            tPreview.BorderStyle = BorderStyle.FixedSingle; tPreview.BackColor = Look.Panel; tPreview.ForeColor = Look.Dim;
            tPreview.Font = Look.Mono; tPreview.WordWrap = false;
            tPreview.Location = new Point(x, y + 15); tPreview.Size = new Size(w, this.ClientSize.Height - (y + 15) - 60);
            this.Controls.Add(tPreview);

            /* templates */
            var tmplLbl = Field("templates:", x, this.ClientSize.Height - 40, 90);
            tmplLbl.ForeColor = Look.Faint;
            var t1 = Btn("sum of a range", x + 88, this.ClientSize.Height - 44, 150, 24, false);
            t1.Click += delegate { LoadTemplate("sumrange"); };
            var t2 = Btn("count matches", x + 246, this.ClientSize.Height - 44, 150, 24, false);
            t2.Click += delegate { LoadTemplate("count"); };
        }

        void BuildRight()
        {
            int x = 516, y = 116, w = 460;
            Section("the node", x, y); y += 30;

            Field("nodes", x, y, w);
            nodeList = new ListBox();
            nodeList.BorderStyle = BorderStyle.FixedSingle; nodeList.BackColor = Look.Panel; nodeList.ForeColor = Look.Ink;
            nodeList.Font = Look.Mono; nodeList.Location = new Point(x, y + 15); nodeList.Size = new Size(w, 92);
            nodeList.IntegralHeight = false;
            nodeList.DoubleClick += delegate { LoadSelectedNode(); };
            nodeList.KeyDown += delegate(object o, KeyEventArgs e) { if (e.KeyCode == Keys.Delete) RemoveSelectedNode(); };
            this.Controls.Add(nodeList);
            RefillNodeList();
            var add = Btn("save node", x, y + 112, 150, 24, false); add.Click += delegate { SaveCurrentNode(); };
            var del = Btn("remove", x + 158, y + 112, 90, 24, false); del.Click += delegate { RemoveSelectedNode(); };
            var test = Btn("test", x + w - 90, y + 112, 90, 24, false); test.Click += delegate { DoProbe(); };
            y += 150;

            Field("host", x, y, w);
            tNode = Text_(x, y + 15, w - 96, "user@host or ssh alias; its door holds your key");
            Field("port", x + w - 88, y, 40); nPort = Num(x + w - 88, y + 14, 88, 1, 65535, 22, "ssh port");
            y += 46;
            Field("key", x, y, w);
            tKey = Text_(x, y + 15, w - 96, "private key file; blank = default keys / agent");
            var kb = Btn("choose", x + w - 90, y + 14, 90, 26, false);
            kb.Click += delegate { var d = new OpenFileDialog(); d.Filter = "ssh key|*|all|*.*"; if (d.ShowDialog() == DialogResult.OK) tKey.Text = d.FileName; };
            y += 46;

            Field("wait", x, y, w);
            nWait = Num(x, y + 15, 90, 0, 300, 8, "seconds to wait before reading the result; 0 = none");
            send = Btn("send", x + 116, y + 12, 200, 34, true); send.Font = Look.Body;
            send.Click += delegate { DoSend(); };
            var savePkg = Btn("save package", x + 328, y + 12, w - 328, 34, false); savePkg.Click += delegate { DoSave(); };
            y += 58;

            Field("output", x, y, w);
            log = new RichTextBox();
            log.BorderStyle = BorderStyle.FixedSingle; log.BackColor = Look.Panel; log.ForeColor = Look.Ink;
            log.Font = Look.Mono; log.ReadOnly = true; log.WordWrap = false; log.ScrollBars = RichTextBoxScrollBars.Both;
            log.Location = new Point(x, y + 15); log.Size = new Size(w, this.ClientSize.Height - (y + 15) - 20);
            this.Controls.Add(log);
            log.Text = "idle.\n";
        }

        CheckBox Check(string text, int x, int y, string tip)
        {
            var c = new CheckBox();
            c.Text = text; c.ForeColor = Look.Ink; c.Font = Look.Body; c.FlatStyle = FlatStyle.Flat;
            c.AutoSize = true; c.Location = new Point(x, y);
            c.CheckedChanged += delegate { RefreshPreview(); };
            if (tip != null) tips.SetToolTip(c, tip);
            this.Controls.Add(c);
            return c;
        }

        NumericUpDown Num(int x, int y, int w, int lo, int hi, int val, string tip)
        {
            var n = new NumericUpDown();
            n.BorderStyle = BorderStyle.FixedSingle; n.BackColor = Look.Field; n.ForeColor = Look.Ink;
            n.Font = Look.Body; n.Minimum = lo; n.Maximum = hi; n.Value = val;
            n.Location = new Point(x, y); n.Size = new Size(w, 24);
            n.ValueChanged += delegate { RefreshPreview(); };
            if (tip != null) tips.SetToolTip(n, tip);
            this.Controls.Add(n);
            return n;
        }

        ComboBox Combo(int x, int y, int w, string[] items, string tip)
        {
            var c = new ComboBox();
            c.DropDownStyle = ComboBoxStyle.DropDownList; c.FlatStyle = FlatStyle.Flat;
            c.BackColor = Look.Field; c.ForeColor = Look.Ink; c.Font = Look.Body;
            c.Location = new Point(x, y); c.Size = new Size(w, 24);
            c.Items.AddRange(items); c.SelectedIndex = 0;
            c.SelectedIndexChanged += delegate { RefreshPreview(); };
            if (tip != null) tips.SetToolTip(c, tip);
            this.Controls.Add(c);
            return c;
        }

        Spec Gather(out string err)
        {
            err = null;
            var s = new Spec();
            s.Source = tSource.Text.Trim(); s.Node = tNode.Text.Trim(); s.Key = tKey.Text.Trim();
            s.Port = (int)nPort.Value; s.Wait = (int)nWait.Value; s.Recipe = kRecipe.Checked;
            s.Name = tName.Text.Trim(); s.InputName = tInputName.Text.Trim();
            s.Combine = cCombine.SelectedItem != null ? cCombine.SelectedItem.ToString() : "sum";
            s.Across = PInt(tAcross.Text); s.Budget = PInt(tBudget.Text); s.Pieces = PInt(tPieces.Text);
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
            if (loading || tPreview == null) return;
            string err; var s = Gather(out err);
            if (s == null) { planLabel.ForeColor = Look.Warn; planLabel.Text = err; return; }

            string manifest = Core.Manifest(s);
            string body = "(choose a source)";
            int bytes = -1;
            if (!string.IsNullOrEmpty(s.Source) && File.Exists(s.Source))
            {
                byte[] pkg = Core.BuildPackage(s, out err);
                if (pkg != null) { bytes = pkg.Length; string full = new UTF8Encoding(false).GetString(pkg); int cut = manifest.Length; body = full.Length > cut ? full.Substring(cut) : ""; }
            }
            tPreview.Text = (manifest + body).Replace("\n", "\r\n");

            int pieces = s.HasSplit ? (s.Pieces > 0 ? s.Pieces : 4) : 1;
            int quorum = s.Across > 0 ? s.Across : 1;
            int slots = pieces * quorum;
            var plan = new StringBuilder();
            if (bytes >= 0) plan.Append(bytes).Append(" B payload · ");
            plan.Append(pieces).Append(pieces == 1 ? " piece" : " pieces");
            if (quorum > 1) plan.Append(" \u00D7 ").Append(quorum).Append(" = ").Append(slots).Append(" slots");
            plan.Append(" · combine ").Append(s.Combine);
            var warn = new StringBuilder();
            if (bytes > Core.WireMax) warn.Append("payload > ").Append(Core.WireMax).Append(" B (wire limit).  ");
            if (slots > Core.SlotMax) warn.Append(slots).Append(" slots > ").Append(Core.SlotMax).Append(" (desk limit).  ");
            if (warn.Length > 0) { planLabel.ForeColor = Look.Warn; planLabel.Text = plan.ToString() + "\n" + warn.ToString(); }
            else { planLabel.ForeColor = Look.Dim; planLabel.Text = plan.ToString(); }
        }

        void Say(string line)
        {
            if (log.InvokeRequired) { log.BeginInvoke((MethodInvoker)delegate { Say(line); }); return; }
            bool hot = line.StartsWith("pipe:") || line.Contains("= ") || line.StartsWith("---") || line.StartsWith("...");
            log.SelectionStart = log.TextLength; log.SelectionColor = hot ? Look.Accent : Look.Ink;
            log.AppendText(line + "\n"); log.SelectionStart = log.TextLength; log.ScrollToCaret();
        }

        void DoSend()
        {
            string err; var s = Gather(out err);
            if (s == null) { Say(err); return; }
            if (string.IsNullOrEmpty(s.Source) || !File.Exists(s.Source)) { Say("no source."); return; }
            if (string.IsNullOrEmpty(s.Node)) { Say("no node."); return; }
            byte[] pkg = Core.BuildPackage(s, out err);
            if (pkg == null) { Say(err); return; }
            if (pkg.Length > Core.WireMax) { Say("payload " + pkg.Length + " B > " + Core.WireMax + " B; not sent."); return; }
            send.Enabled = false;
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

        void DoSave()
        {
            string err; var s = Gather(out err);
            if (s == null) { Say(err); return; }
            if (string.IsNullOrEmpty(s.Source) || !File.Exists(s.Source)) { Say("no source."); return; }
            byte[] pkg = Core.BuildPackage(s, out err);
            if (pkg == null) { Say(err); return; }
            var d = new SaveFileDialog(); d.Filter = "task package|*.ebtask|all|*.*";
            d.FileName = (string.IsNullOrEmpty(s.Name) ? "task" : s.Name) + ".ebtask";
            if (d.ShowDialog() == DialogResult.OK) { File.WriteAllBytes(d.FileName, pkg); Say("wrote " + d.FileName + " (" + pkg.Length + " bytes)"); }
        }

        void LoadTemplate(string which)
        {
            string dir = Path.Combine(Path.GetTempPath(), "EreBUS Gate");
            try { Directory.CreateDirectory(dir); } catch { }
            if (which == "sumrange")
            {
                string src = "long main(long c, long n) {\n" +
                             "    long b[16]; long lo; long hi; long s; long i;\n" +
                             "    syscall(3, n, b, 0, 0, 0);\n" +
                             "    lo = b[2]; hi = b[3]; s = 0;\n" +
                             "    for (i = lo; i <= hi; i = i + 1) s = s + i;\n" +
                             "    syscall(2, c, 0x54584554, s, 0, 0);\n" +
                             "    return 0;\n}\n";
                string path = Path.Combine(dir, "sumrange.c"); File.WriteAllText(path, src);
                loading = true;
                tSource.Text = path; kRecipe.Checked = false; tName.Text = "sumrange";
                kSplit.Checked = true; tLo.Text = "1"; tHi.Text = "1000000"; tPieces.Text = "8";
                cCombine.SelectedItem = "sum"; tAcross.Text = ""; tBudget.Text = "15";
                loading = false; RefreshPreview();
                Say("loaded sumrange: 1..1000000, 8 pieces, sum.");
            }
            else if (which == "count")
            {
                string src = "long main(long c, long n) {\n" +
                             "    long b[16]; long lo; long hi; long k; long i;\n" +
                             "    syscall(3, n, b, 0, 0, 0);\n" +
                             "    lo = b[2]; hi = b[3]; k = 0;\n" +
                             "    for (i = lo; i <= hi; i = i + 1) if ((i & 7) == 0) k = k + 1;\n" +
                             "    syscall(2, c, 0x54584554, k, 0, 0);\n" +
                             "    return 0;\n}\n";
                string path = Path.Combine(dir, "count.c"); File.WriteAllText(path, src);
                loading = true;
                tSource.Text = path; kRecipe.Checked = false; tName.Text = "count";
                kSplit.Checked = true; tLo.Text = "1"; tHi.Text = "1000000"; tPieces.Text = "8";
                cCombine.SelectedItem = "sum"; tBudget.Text = "15";
                loading = false; RefreshPreview();
                Say("loaded count: multiples of 8 in 1..1000000, sum.");
            }
        }

        void RefillNodeList()
        {
            nodeList.Items.Clear();
            foreach (var n in nodes) nodeList.Items.Add(n);
        }
        void LoadSelectedNode()
        {
            var n = nodeList.SelectedItem as NodeEntry; if (n == null) return;
            tNode.Text = n.Host; nPort.Value = Math.Max(1, Math.Min(65535, n.Port)); tKey.Text = n.Key;
        }
        void SaveCurrentNode()
        {
            string host = tNode.Text.Trim(); if (host.Length == 0) { Say("no host."); return; }
            NodeEntry found = null;
            foreach (var n in nodes) if (n.Host == host) { found = n; break; }
            if (found == null) { found = new NodeEntry(); nodes.Add(found); }
            found.Host = host; found.Name = host; found.Port = (int)nPort.Value; found.Key = tKey.Text.Trim();
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
            if (last.ContainsKey("combine") && cCombine.Items.Contains(last["combine"])) cCombine.SelectedItem = last["combine"];
            int v;
            if (last.ContainsKey("port") && int.TryParse(last["port"], out v)) nPort.Value = Math.Max(1, Math.Min(65535, v));
            if (last.ContainsKey("wait") && int.TryParse(last["wait"], out v)) nWait.Value = Math.Max(0, Math.Min(300, v));
            loading = false;
        }
        void Persist()
        {
            last["node"] = tNode != null ? tNode.Text.Trim() : "";
            last["key"] = tKey != null ? tKey.Text.Trim() : "";
            last["combine"] = cCombine != null && cCombine.SelectedItem != null ? cCombine.SelectedItem.ToString() : "sum";
            last["port"] = nPort != null ? ((int)nPort.Value).ToString() : "22";
            last["wait"] = nWait != null ? ((int)nWait.Value).ToString() : "8";
            Store.Save(nodes, last);
        }

        void ShowAbout()
        {
            MessageBox.Show(
                "EreBUS Gate " + Ver.V + "\n\n" +
                "packages a task, feeds it to a node's desk over ssh.\n" +
                "payload: EreBUS c (far-task ABI) or a recipe, <= 1 KiB.\n" +
                "the node's door must hold your ssh key.",
                "EreBUS Gate", MessageBoxButtons.OK, MessageBoxIcon.None);
        }
    }

    static class Program
    {
        [System.Runtime.InteropServices.DllImport("kernel32.dll")]
        static extern bool AttachConsole(int pid);

        [STAThread]
        static int Main(string[] args)
        {
            if (args.Length > 0) AttachConsole(-1);
            if (Has(args, "--version")) { Console.WriteLine("EreBUS Gate " + Ver.V); return 0; }
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
    }
}
