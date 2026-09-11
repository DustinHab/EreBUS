/*
 * EreBUS Gate -- a Windows front-end and machine interface for an EreBUS
 * far-work cluster. Seven views over the node's control channel: overview,
 * nodes, tasks, jobs, logs, cluster and settings; and "api <verb>" for a
 * program (json on stdout).
 *
 * No SDK: build with the .NET Framework compiler.
 *   tools\gate\build.cmd            -> build\gate\EreBUS-Gate.exe      the window
 *                                   -> build\gate\EreBUS-Gate-api.exe  the machine interface
 * The two are one source: the window build opens the window when it is given
 * no arguments, the console build never does and is the one a shell can
 * redirect or pipe (Windows gives a window program no standard handles).
 * --headless builds/feeds a package from a command line; --shot <file.png>
 * [--view <name>] renders a view with made-up nodes, for documentation -- it
 * asks no node and shows no live one.
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
[assembly: System.Reflection.AssemblyVersion("0.2.0.0")]
[assembly: System.Reflection.AssemblyFileVersion("0.2.0.0")]

namespace EreBUSGate
{
    static class Ver { public const string V = "0.2"; }

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
        public static Font Nav   = new Font("Consolas", 10.5f, FontStyle.Regular, GraphicsUnit.Point);
        public static Font Big   = new Font("Consolas", 19f,   FontStyle.Bold,    GraphicsUnit.Point);

        public static string Spaced(string s) { return s; }   /* labels stay plain lowercase, no caps, no letter-spacing */
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

    /* A dark, drawn table: columns and string-row data, optional per-cell
     * colour, hover to scroll, click to select. No white chrome. */
    class Grid : Panel
    {
        public class Column { public string Name; public int W; public Column(string n, int w) { Name = n; W = w; } }
        public delegate Color Painter(int row, int col, string text);

        List<Column> cols = new List<Column>();
        List<string[]> rows = new List<string[]>();
        public Painter Tint;
        int top, sel = -1;
        public event EventHandler Chosen;
        const int RowH = 22, HeadH = 24, Pad = 12;

        public Grid()
        {
            BackColor = Look.Panel;
            this.SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.Selectable, true);
            this.TabStop = false;
            this.MouseEnter += delegate { try { this.Focus(); } catch { } };
        }

        public void Columns(Column[] c) { cols = new List<Column>(c); Invalidate(); }
        public void SetRows(List<string[]> r) { rows = r ?? new List<string[]>(); if (sel >= rows.Count) sel = -1; Clamp(); Invalidate(); }
        public string[] Current { get { return (sel >= 0 && sel < rows.Count) ? rows[sel] : null; } }
        public int Count { get { return rows.Count; } }

        void Clamp() { int vis = Math.Max(1, (Height - HeadH) / RowH); int max = Math.Max(0, rows.Count - vis); if (top > max) top = max; if (top < 0) top = 0; }

        protected override void OnMouseWheel(MouseEventArgs e) { top -= (e.Delta > 0 ? 3 : -3); Clamp(); Invalidate(); base.OnMouseWheel(e); }
        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);
            try { this.Focus(); } catch { }
            if (e.Y < HeadH) return;
            int i = top + (e.Y - HeadH) / RowH;
            if (i >= 0 && i < rows.Count) { sel = i; Invalidate(); if (Chosen != null) Chosen(this, EventArgs.Empty); }
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.Clear(Look.Panel);
            g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
            int x = Pad;
            using (var hb = new SolidBrush(Look.Accent))
                foreach (var c in cols) { g.DrawString(Look.Spaced(c.Name), Look.Small, hb, x, 5); x += c.W; }
            using (var pen = new Pen(Look.Edge)) g.DrawLine(pen, Pad, HeadH - 2, Width - Pad, HeadH - 2);

            int vis = (Height - HeadH) / RowH;
            for (int r = 0; r < vis; r++)
            {
                int ri = top + r; if (ri >= rows.Count) break;
                int y = HeadH + r * RowH;
                if (ri == sel) using (var sb = new SolidBrush(Look.Field)) g.FillRectangle(sb, Pad - 5, y, Width - 2 * Pad + 10, RowH);
                var row = rows[ri]; x = Pad;
                for (int c = 0; c < cols.Count; c++)
                {
                    string t = (c < row.Length) ? (row[c] ?? "") : "";
                    Color col = Tint != null ? Tint(ri, c, t) : Look.Ink;
                    using (var cb = new SolidBrush(col)) g.DrawString(t, Look.Mono, cb, x, y + 3);
                    x += cols[c].W;
                }
            }
            if (rows.Count == 0)
                using (var db = new SolidBrush(Look.Faint)) g.DrawString("(nothing yet)", Look.Mono, db, Pad, HeadH + 4);

            if (rows.Count > vis)
            {
                int trackH = Height - HeadH;
                int thumbH = Math.Max(20, trackH * vis / rows.Count);
                int thumbY = HeadH + (trackH - thumbH) * top / Math.Max(1, rows.Count - vis);
                using (var tb = new SolidBrush(Look.Edge)) g.FillRectangle(tb, Width - 5, thumbY, 3, thumbH);
            }
        }
    }

    /* A stat tile: a big value over a spaced-caps label, in a drawn box. */
    class Tile : Panel
    {
        public string Label = "", Value = "-";
        public Color ValueColor = Look.Ink;
        public Tile()
        {
            BackColor = Look.Ground;
            this.SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint, true);
        }
        public void Set(string v, Color c) { Value = v; ValueColor = c; Invalidate(); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.Clear(Look.Ground);
            g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
            using (var pen = new Pen(Look.Edge)) g.DrawRectangle(pen, 0, 0, Width - 1, Height - 1);
            using (var vb = new SolidBrush(ValueColor)) g.DrawString(Value, Look.Big, vb, 12, 11);
            using (var lb = new SolidBrush(Look.Dim)) g.DrawString(Look.Spaced(Label), Look.Small, lb, 13, Height - 20);
        }
    }

    /* A dark, drawn, read-only text pane: renders its lines itself (so it
     * shows in a screenshot, unlike a native text box), pins to the last
     * line, hover to scroll. "= ", "---" and "..." lines take the accent. */
    class TextPane : Panel
    {
        List<string> lines = new List<string>(); int top;
        public TextPane()
        {
            BackColor = Look.Panel;
            this.SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.Selectable, true);
            this.TabStop = false;
            this.MouseEnter += delegate { try { this.Focus(); } catch { } };
        }
        int Vis() { return Math.Max(1, (Height - 6) / 17); }
        void ToEnd() { top = Math.Max(0, lines.Count - Vis()); }
        public void SetText(string s) { lines = new List<string>((s ?? "").Replace("\r", "").Split('\n')); ToEnd(); Invalidate(); }
        public void Append(string line) { lines.Add(line); if (lines.Count > 800) lines.RemoveRange(0, lines.Count - 800); ToEnd(); Invalidate(); }
        protected override void OnMouseWheel(MouseEventArgs e) { top -= (e.Delta > 0 ? 3 : -3); top = Math.Max(0, Math.Min(top, Math.Max(0, lines.Count - Vis()))); Invalidate(); base.OnMouseWheel(e); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.Clear(Look.Panel);
            g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
            int vis = Vis(), y = 4;
            for (int i = 0; i < vis; i++)
            {
                int li = top + i; if (li >= lines.Count) break;
                string t = lines[li];
                bool hot = t.StartsWith("=") || t.StartsWith("---") || t.StartsWith("...");
                using (var b = new SolidBrush(hot ? Look.Accent : Look.Ink)) g.DrawString(t, Look.Mono, b, 6, y);
                y += 17;
            }
        }
    }

    class GateForm : Form
    {
        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
        static extern int SetWindowTheme(IntPtr h, string app, string id);

        readonly bool demo;
        List<NodeEntry> nodes = new List<NodeEntry>();
        Dictionary<string, string> last = new Dictionary<string, string>();
        NodeEntry target;

        Point dragFrom; bool dragging;
        Panel content;
        Dictionary<string, Panel> views = new Dictionary<string, Panel>();
        List<Label> navItems = new List<Label>();
        string active = "overview";
        Label statusLbl, targetLbl;
        List<Control> bordered = new List<Control>();
        ToolTip tips = new ToolTip();

        static readonly string[] Views = { "overview", "nodes", "tasks", "jobs", "logs", "cluster", "settings" };

        const int NavW = 156, HeadH = 66, StatH = 26;

        public GateForm() : this(false) { }
        public GateForm(bool demo)
        {
            this.demo = demo;
            Store.Load(nodes, last);
            /* A picture for the documentation shows the made-up cluster and
             * nothing of whoever renders it: the saved nodes are set aside,
             * so no real name or address ends up in a published screenshot,
             * and the tiles, the table and the header say one thing. */
            if (demo) { nodes.Clear(); SeedDemo(); }
            target = PickTarget();

            this.Text = "EreBUS Gate";
            this.FormBorderStyle = FormBorderStyle.None;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Look.Ground; this.ForeColor = Look.Ink; this.Font = Look.Body;
            this.ClientSize = new Size(1140, 760);
            this.KeyPreview = true;
            this.Paint += delegate(object o, PaintEventArgs e)
            {
                using (var pen = new Pen(Look.Edge))
                {
                    e.Graphics.DrawRectangle(pen, 0, 0, this.Width - 1, this.Height - 1);
                    e.Graphics.DrawLine(pen, NavW, HeadH, NavW, this.Height - StatH);
                    e.Graphics.DrawLine(pen, 1, HeadH, this.Width - 2, HeadH);
                    e.Graphics.DrawLine(pen, 1, this.Height - StatH, this.Width - 2, this.Height - StatH);
                    foreach (Control c in bordered)
                        if (c.Visible) e.Graphics.DrawRectangle(pen, c.Left - 1, c.Top - 1, c.Width + 1, c.Height + 1);
                }
            };
            this.KeyDown += delegate(object o, KeyEventArgs e) { if (e.KeyCode == Keys.Escape) this.Close(); };
            this.FormClosing += delegate { Persist(); };

            BuildHeader();
            BuildNav();
            BuildStatus();

            content = new Panel();
            content.Location = new Point(NavW + 1, HeadH + 1);
            content.Size = new Size(this.ClientSize.Width - NavW - 2, this.ClientSize.Height - HeadH - StatH - 2);
            content.BackColor = Look.Ground;
            this.Controls.Add(content);

            BuildOverview(); BuildNodes(); BuildTasks(); BuildJobs(); BuildLogs(); BuildCluster(); BuildSettings();
            ShowView(active);
            if (demo) Say("sample data for a picture; no node was asked.");
        }

        void Dark(Control c) { try { SetWindowTheme(c.Handle, "DarkMode_Explorer", null); } catch { } }

        /* ---- chrome ---------------------------------------------------- */

        void BuildHeader()
        {
            var head = new Panel();
            head.Bounds = new Rectangle(1, 1, this.ClientSize.Width - 2, HeadH - 1);
            head.BackColor = Look.Ground;
            head.Paint += delegate(object o, PaintEventArgs e)
            {
                var g = e.Graphics; g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
                using (var b = new SolidBrush(Look.Accent)) g.DrawString("EreBUS Gate", Look.Head, b, 20, 12);
                using (var b = new SolidBrush(Look.Dim)) g.DrawString("v " + Ver.V, Look.Small, b, 23, 44);
            };
            head.MouseDown += delegate(object o, MouseEventArgs e) { dragging = true; dragFrom = e.Location; };
            head.MouseUp += delegate { dragging = false; };
            head.MouseMove += delegate(object o, MouseEventArgs e)
            { if (dragging) this.Location = new Point(this.Location.X + e.X - dragFrom.X, this.Location.Y + e.Y - dragFrom.Y); };
            this.Controls.Add(head);

            head.Controls.Add(Glyph("\u00D7", head.Width - 34, 20, delegate { this.Close(); }));
            head.Controls.Add(Glyph("\u2013", head.Width - 66, 20, delegate { this.WindowState = FormWindowState.Minimized; }));

            targetLbl = new Label();
            targetLbl.AutoSize = false; targetLbl.Size = new Size(300, 26); targetLbl.TextAlign = ContentAlignment.MiddleRight;
            targetLbl.Location = new Point(head.Width - 300 - 84, 20); targetLbl.Font = Look.Body; targetLbl.ForeColor = Look.Dim;
            targetLbl.Cursor = Cursors.Hand;
            targetLbl.Click += delegate { CycleTarget(); };
            tips.SetToolTip(targetLbl, "the node the views act on; click to cycle through saved nodes");
            head.Controls.Add(targetLbl);
            RenderTarget();
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

        void BuildNav()
        {
            int y = HeadH + 22;
            foreach (var name in Views)
            {
                string vn = name;
                var l = new Label();
                l.Text = "  " + Look.Spaced(name); l.Font = Look.Nav; l.ForeColor = Look.Dim;
                l.AutoSize = false; l.Size = new Size(NavW - 6, 30); l.TextAlign = ContentAlignment.MiddleLeft;
                l.Location = new Point(4, y); l.Cursor = Cursors.Hand;
                l.Paint += delegate(object o, PaintEventArgs e)
                {
                    if (active == vn)
                        using (var b = new SolidBrush(Look.Accent)) e.Graphics.FillRectangle(b, 0, 4, 3, l.Height - 8);
                };
                l.MouseEnter += delegate { if (active != vn) l.ForeColor = Look.Ink; };
                l.MouseLeave += delegate { if (active != vn) l.ForeColor = Look.Dim; };
                l.Click += delegate { ShowView(vn); };
                this.Controls.Add(l);
                navItems.Add(l);
                y += 34;
            }
        }

        void BuildStatus()
        {
            statusLbl = new Label();
            statusLbl.AutoSize = false; statusLbl.Location = new Point(12, this.ClientSize.Height - StatH + 4);
            statusLbl.Size = new Size(this.ClientSize.Width - 24, 18); statusLbl.Font = Look.Small; statusLbl.ForeColor = Look.Dim;
            this.Controls.Add(statusLbl);
            Say("ready.");
        }

        void Say(string s)
        {
            if (statusLbl == null) return;
            if (statusLbl.InvokeRequired) { statusLbl.BeginInvoke((MethodInvoker)delegate { Say(s); }); return; }
            statusLbl.Text = s;
        }

        void ShowView(string name)
        {
            active = name;
            foreach (var kv in views) kv.Value.Visible = (kv.Key == name);
            foreach (var l in navItems) { bool on = ("  " + Look.Spaced(name)) == l.Text; l.ForeColor = on ? Look.Accent : Look.Dim; l.Invalidate(); }
            if (!demo) OnEnterView(name);
        }
        public void Show(string name) { ShowView(name); }  /* for the shot driver */

        /* ---- the target node ------------------------------------------ */

        NodeEntry PickTarget()
        {
            string want = last.ContainsKey("target") ? last["target"] : null;
            if (want != null) foreach (var n in nodes) if (n.Host == want || n.Name == want) return n;
            return nodes.Count > 0 ? nodes[0] : null;
        }
        void RenderTarget()
        {
            if (targetLbl == null) return;
            targetLbl.Text = "target \u00b7 " + (target != null ? target.ToString() : "(none)") + "   ";
        }
        void SetTarget(NodeEntry n) { target = n; if (n != null) last["target"] = n.Host; RenderTarget(); }
        void CycleTarget()
        {
            if (nodes.Count == 0) return;
            int i = target != null ? nodes.IndexOf(target) : -1;
            SetTarget(nodes[(i + 1) % nodes.Count]);
            if (!demo) OnEnterView(active);
        }
        Spec SpecOf(NodeEntry n)
        {
            var s = new Spec();
            if (n != null) { s.Node = n.Host; s.Port = n.Port < 1 ? 22 : n.Port; s.Key = n.Key; }
            return s;
        }

        /* ---- styled controls ------------------------------------------ */

        Label Section(Control host, string text, int x, int y, int w)
        {
            var l = new Label();
            l.Text = Look.Spaced(text); l.Font = Look.Small; l.ForeColor = Look.Accent;
            l.AutoSize = true; l.Location = new Point(x, y); host.Controls.Add(l);
            var rule = new Label(); rule.AutoSize = false; rule.BackColor = Look.Edge;
            rule.Bounds = new Rectangle(x, y + 18, w, 1); host.Controls.Add(rule);
            return l;
        }

        Label Field(Control host, string text, int x, int y, int w)
        {
            var l = new Label();
            l.Text = text; l.Font = Look.Small; l.ForeColor = Look.Dim;
            l.AutoSize = false; l.Location = new Point(x, y); l.Size = new Size(w, 15);
            host.Controls.Add(l); return l;
        }

        TextBox Text_(Control host, int x, int y, int w, string tip)
        {
            var t = new TextBox();
            t.BorderStyle = BorderStyle.None; t.BackColor = Look.Field; t.ForeColor = Look.Ink;
            t.Font = Look.Body; t.Location = new Point(x + 3, y + 4); t.Size = new Size(w - 6, 20);
            var box = new Panel(); box.BackColor = Look.Field; box.Location = new Point(x, y); box.Size = new Size(w, 26);
            box.Controls.Add(t); host.Controls.Add(box); bordered.Add(box);
            if (tip != null) tips.SetToolTip(t, tip);
            return t;
        }

        Button Btn(Control host, string text, int x, int y, int w, int h, bool strong)
        {
            var b = new Button();
            b.Text = text; b.Font = strong ? Look.Body : Look.Mono;
            b.FlatStyle = FlatStyle.Flat; b.FlatAppearance.BorderColor = strong ? Look.Accent : Look.Edge;
            b.FlatAppearance.BorderSize = 1; b.BackColor = Look.Ground;
            b.ForeColor = strong ? Look.Accent : Look.Dim;
            b.FlatAppearance.MouseOverBackColor = Color.FromArgb(0x2C, 0x23, 0x1B);
            b.Location = new Point(x, y); b.Size = new Size(w, h); b.Cursor = Cursors.Hand;
            b.UseCompatibleTextRendering = true;
            host.Controls.Add(b); return b;
        }

        Grid MakeGrid(Control host, int x, int y, int w, int h)
        {
            var grid = new Grid();
            grid.Location = new Point(x, y); grid.Size = new Size(w, h);
            var box = new Panel(); box.Location = new Point(x - 1, y - 1); box.Size = new Size(w + 2, h + 2); box.BackColor = Look.Panel;
            host.Controls.Add(box); box.Controls.Add(grid); grid.Location = new Point(1, 1);
            bordered.Add(box);
            return grid;
        }

        Panel View(string name)
        {
            var p = new Panel();
            p.Location = new Point(0, 0); p.Size = content.Size; p.BackColor = Look.Ground; p.Visible = false;
            content.Controls.Add(p); views[name] = p;
            return p;
        }

        /* ---- background helpers --------------------------------------- */

        void Bg(ThreadStart work) { var t = new Thread(work); t.IsBackground = true; t.Start(); }
        void UI(MethodInvoker m) { if (this.IsDisposed) return; if (this.InvokeRequired) { try { this.BeginInvoke(m); } catch { } } else m(); }

        Ctl.Frm OneShot(NodeEntry n, string method, byte[] body, out string err)
        {
            err = null;
            if (n == null) { err = "no target node"; return null; }
            var c = Ctl.Open(SpecOf(n), out err);
            if (c == null) return null;
            var f = c.Req(method, body); c.Close();
            return f;
        }
        static string KV(byte[] body, string k) { return Ctl.KV(body, k); }

        /* Split a "k=v k=v ... text=rest" control line into a dictionary. */
        static Dictionary<string, string> Parse(string line)
        {
            var d = new Dictionary<string, string>();
            string head = line, text = null;
            int ti = line.IndexOf(" text=");
            if (ti >= 0) { text = line.Substring(ti + 6); head = line.Substring(0, ti); }
            foreach (var tok in head.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries))
            { int eq = tok.IndexOf('='); if (eq > 0) d[tok.Substring(0, eq)] = tok.Substring(eq + 1); }
            if (text != null) d["text"] = text;
            return d;
        }
        static string Get(Dictionary<string, string> d, string k) { return d.ContainsKey(k) ? d[k] : ""; }

        void OnEnterView(string name)
        {
            if (name == "overview") RefreshOverview();
            else if (name == "jobs") RefreshJobs();
            else if (name == "logs") RefreshLog();
            else if (name == "cluster") RefreshCluster();
        }

        /* ================= OVERVIEW ===================================== */

        Tile tNodes, tOnline, tJobs, tGate;
        Grid ovGrid; Toggle ovAuto; System.Windows.Forms.Timer ovTimer;

        void BuildOverview()
        {
            var p = View("overview");
            Section(p, "cluster overview", 22, 16, p.Width - 44);
            int tw = (p.Width - 44 - 3 * 14) / 4, ty = 48, th = 66;
            tNodes = Tl(p, 22, ty, tw, th, "nodes");
            tOnline = Tl(p, 22 + (tw + 14), ty, tw, th, "online");
            tJobs = Tl(p, 22 + 2 * (tw + 14), ty, tw, th, "jobs in flight");
            tGate = Tl(p, 22 + 3 * (tw + 14), ty, tw, th, "target");

            Btn(p, "refresh", 22, ty + th + 14, 100, 26, false).Click += delegate { RefreshOverview(); };
            ovAuto = new Toggle("auto"); ovAuto.Location = new Point(134, ty + th + 18); p.Controls.Add(ovAuto);
            ovAuto.Changed += delegate { if (ovAuto.Checked) ovTimer.Start(); else ovTimer.Stop(); };

            ovGrid = MakeGrid(p, 22, ty + th + 50, p.Width - 44, p.Height - (ty + th + 50) - 16);
            ovGrid.Columns(new Grid.Column[] { new Grid.Column("node", 200), new Grid.Column("state", 100), new Grid.Column("version", 220), new Grid.Column("uptime", 120), new Grid.Column("jobs", 80) });
            ovGrid.Tint = delegate(int r, int c, string t) { return (c == 1) ? (t == "online" ? Look.Accent : Look.Dim) : Look.Ink; };
            ovGrid.Chosen += delegate { var row = ovGrid.Current; if (row != null) { var n = ByName(row[0]); if (n != null) { SetTarget(n); OnEnterView(active); } } };

            ovTimer = new System.Windows.Forms.Timer(); ovTimer.Interval = 6000; ovTimer.Tick += delegate { RefreshOverview(); };
            this.FormClosing += delegate { ovTimer.Stop(); };
            if (demo) SeedOverview();
        }

        Tile Tl(Control host, int x, int y, int w, int h, string label)
        {
            var t = new Tile(); t.Label = label; t.Location = new Point(x, y); t.Size = new Size(w, h); host.Controls.Add(t); return t;
        }

        void RefreshOverview()
        {
            if (demo) return;
            var list = new List<NodeEntry>(nodes);
            Say("polling " + list.Count + " node(s) ...");
            Bg(delegate
            {
                var rows = new List<string[]>();
                int online = 0; long jobs = 0;
                foreach (var n in list)
                {
                    string err; var f = OneShot(n, "status", null, out err);
                    if (f != null && f.Kind == 2)
                    {
                        online++;
                        string ver = KV(f.Body, "version"); string up = KV(f.Body, "uptime_s"); string jb = KV(f.Body, "jobs");
                        long jv; if (long.TryParse(jb ?? "", out jv)) jobs += jv;
                        rows.Add(new string[] { n.ToString(), "online", ver ?? "-", Uptime(up), jb ?? "-" });
                    }
                    else rows.Add(new string[] { n.ToString(), "offline", "-", "-", "-" });
                }
                long jf = jobs; int on = online;
                UI(delegate
                {
                    ovGrid.SetRows(rows);
                    tNodes.Set(list.Count.ToString(), Look.Ink);
                    tOnline.Set(on.ToString(), on > 0 ? Look.Accent : Look.Dim);
                    tJobs.Set(jf.ToString(), jf > 0 ? Look.Accent : Look.Ink);
                    tGate.Set(target != null ? target.ToString() : "-", Look.Ink);
                    Say("overview: " + on + " of " + list.Count + " online.");
                });
            });
        }

        static string Uptime(string secs)
        {
            long s; if (!long.TryParse(secs ?? "", out s)) return "-";
            if (s < 60) return s + "s";
            if (s < 3600) return (s / 60) + "m";
            if (s < 86400) return (s / 3600) + "h " + ((s % 3600) / 60) + "m";
            return (s / 86400) + "d " + ((s % 86400) / 3600) + "h";
        }

        void SeedOverview()
        {
            ovGrid.SetRows(new List<string[]> {
                new string[]{"alpha","online","0.9.6","2h 14m","0"},
                new string[]{"beta","online","0.9.6","2h 09m","1"},
                new string[]{"gamma","online","0.9.6","41m","0"},
                new string[]{"delta","offline","-","-","-"},
            });
            tNodes.Set("4", Look.Ink); tOnline.Set("3", Look.Accent); tJobs.Set("1", Look.Accent); tGate.Set("beta", Look.Ink);
        }

        /* ================= NODES ======================================== */

        Grid ndGrid; TextBox ndName, ndHost, ndPort, ndKey; Label ndReadout;

        void BuildNodes()
        {
            var p = View("nodes");
            Section(p, "nodes", 22, 16, 520);
            ndGrid = MakeGrid(p, 22, 48, 520, 260);
            ndGrid.Columns(new Grid.Column[] { new Grid.Column("name", 160), new Grid.Column("host", 240), new Grid.Column("port", 80) });
            ndGrid.Chosen += delegate { LoadSelectedNode(); };
            RefillNodes();

            Btn(p, "use as target", 22, 320, 130, 26, true).Click += delegate { var n = SelectedNode(); if (n != null) { SetTarget(n); Say("target is " + n + "."); } };
            Btn(p, "test", 160, 320, 80, 26, false).Click += delegate { TestNode(); };
            Btn(p, "remove", 248, 320, 90, 26, false).Click += delegate { RemoveNode(); };

            int fx = 566, fw = p.Width - fx - 22;
            Section(p, "edit", fx, 16, fw);
            Field(p, "name", fx, 46, fw); ndName = Text_(p, fx, 61, fw, "a label for this node");
            Field(p, "host", fx, 92, fw); ndHost = Text_(p, fx, 107, fw - 96, "user@host or an ssh alias");
            Field(p, "port", fx + fw - 88, 92, 88); ndPort = Text_(p, fx + fw - 88, 107, 88, "ssh port");
            Field(p, "key", fx, 138, fw); ndKey = Text_(p, fx, 153, fw - 96, "private key file; blank = default keys / agent");
            Btn(p, "choose", fx + fw - 90, 152, 90, 26, false).Click += delegate { var d = new OpenFileDialog(); d.Filter = "ssh key|*|all|*.*"; if (d.ShowDialog() == DialogResult.OK) ndKey.Text = d.FileName; };
            Btn(p, "save node", fx, 190, 120, 28, true).Click += delegate { SaveNode(); };
            Btn(p, "scan for peers", fx + 130, 190, 140, 28, false).Click += delegate { ScanNode(); };
            Btn(p, "door line", fx + 280, 190, 110, 28, false).Click += delegate { DoorLine(); };

            Field(p, "readout", fx, 232, fw);
            ndReadout = new Label(); ndReadout.AutoSize = false; ndReadout.Location = new Point(fx, 250); ndReadout.Size = new Size(fw, p.Height - 250 - 16);
            ndReadout.Font = Look.Mono; ndReadout.ForeColor = Look.Ink; p.Controls.Add(ndReadout);
        }

        void RefillNodes()
        {
            var rows = new List<string[]>();
            foreach (var n in nodes) rows.Add(new string[] { n.Name, n.Host, n.Port.ToString() });
            ndGrid.SetRows(rows);
        }
        NodeEntry SelectedNode() { var r = ndGrid.Current; return r != null ? ByHost(r[1]) : null; }
        NodeEntry ByHost(string h) { foreach (var n in nodes) if (n.Host == h) return n; return null; }
        NodeEntry ByName(string nm) { foreach (var n in nodes) if (n.ToString() == nm) return n; return null; }
        void LoadSelectedNode()
        {
            var n = SelectedNode(); if (n == null) return;
            ndName.Text = n.Name; ndHost.Text = n.Host; ndPort.Text = n.Port.ToString(); ndKey.Text = n.Key;
        }
        void SaveNode()
        {
            string host = ndHost.Text.Trim(); if (host.Length == 0) { Say("no host."); return; }
            NodeEntry found = null; foreach (var n in nodes) if (n.Host == host) { found = n; break; }
            if (found == null) { found = new NodeEntry(); nodes.Add(found); }
            found.Host = host; found.Name = ndName.Text.Trim().Length > 0 ? ndName.Text.Trim() : host;
            int.TryParse(ndPort.Text.Trim(), out found.Port); if (found.Port < 1) found.Port = 22;
            found.Key = ndKey.Text.Trim();
            RefillNodes(); Persist(); if (target == null) SetTarget(found); Say("saved " + found + ".");
        }
        void RemoveNode()
        {
            var n = SelectedNode(); if (n == null) return;
            nodes.Remove(n); if (target == n) SetTarget(nodes.Count > 0 ? nodes[0] : null);
            RefillNodes(); Persist(); Say("removed.");
        }
        void TestNode()
        {
            var n = SelectedNode() ?? target; if (n == null) { Say("select a node."); return; }
            ndReadout.ForeColor = Look.Dim; ndReadout.Text = "testing " + n + " ...";
            Bg(delegate
            {
                string err; var f = OneShot(n, "status", null, out err);
                string txt = (f != null && f.Kind == 2)
                    ? "online\nversion  " + (KV(f.Body, "version") ?? "-") + "\nuptime   " + Uptime(KV(f.Body, "uptime_s")) + "\njobs     " + (KV(f.Body, "jobs") ?? "-")
                    : "offline\n" + (err ?? "no control channel");
                UI(delegate { ndReadout.ForeColor = Look.Ink; ndReadout.Text = txt; Say("tested " + n + "."); });
            });
        }
        void ScanNode()
        {
            var n = SelectedNode() ?? target; if (n == null) { Say("select a node."); return; }
            ndReadout.ForeColor = Look.Dim; ndReadout.Text = "scanning from " + n + " ...";
            Bg(delegate
            {
                string err; var f = OneShot(n, "peers", null, out err);
                var sb = new StringBuilder();
                if (f != null && f.Kind == 2)
                {
                    foreach (var line in Encoding.UTF8.GetString(f.Body).Split('\n'))
                    {
                        if (line.Length == 0) continue;
                        if (line.StartsWith("count=")) { sb.Append(line.Substring(6)).Append(" peer(s) heard\n"); continue; }
                        var d = Parse(line);
                        sb.Append("  ").Append(Get(d, "name")).Append("  ").Append(Get(d, "ip")).Append("  ").Append(Get(d, "version")).Append('\n');
                    }
                }
                else sb.Append(err ?? "no control channel");
                UI(delegate { ndReadout.ForeColor = Look.Ink; ndReadout.Text = sb.ToString(); Say("scanned."); });
            });
        }
        void DoorLine()
        {
            var d = new OpenFileDialog(); d.Filter = "ssh public key|*.pub|all|*.*"; d.Title = "pick your ssh public key";
            if (d.ShowDialog() != DialogResult.OK) return;
            try
            {
                string[] parts = File.ReadAllText(d.FileName).Trim().Split(new char[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length < 2) { Say("that does not look like an ssh public key."); return; }
                string line = "write door | " + parts[0] + " " + parts[1];
                Clipboard.SetText(line);
                ndReadout.ForeColor = Look.Ink;
                ndReadout.Text = "door line copied to the clipboard:\n\n" + line + "\n\non the node: go system, go settings, paste it, back.";
                Say("door line copied.");
            }
            catch (Exception e) { Say("cannot read the key: " + e.Message); }
        }

        /* ================= TASKS ======================================== */

        TextBox tEditor, tName, tLo, tHi, tPieces, tAcross, tBudget, tInput, tWait, tResult;
        Toggle kSplit, kRecipe;
        Button[] combineBtns; string combineSel = "sum";
        Label planLbl, gaugeLbl; TextPane tOut; Button sendBtn; bool loading;

        void BuildTasks()
        {
            var p = View("tasks");
            int lw = 470, x = 22, y = 16;
            Section(p, "task", x, y, lw); y += 30;

            kRecipe = MkToggle(p, "recipe", x, y, "checked: recipe for the interpreter; unchecked: c the node compiles");
            Field(p, "name", x + 250, y - 2, 60); tName = Text_(p, x + 250, y + 12, lw - 250, "task name, for the log");
            y += 42;

            kSplit = MkToggle(p, "split", x, y, "divide lo..hi into pieces, one per machine"); y += 26;
            Field(p, "from", x, y, 60); tLo = Text_(p, x, y + 15, 120, "low end");
            Field(p, "to", x + 132, y, 40); tHi = Text_(p, x + 132, y + 15, 120, "high end");
            Field(p, "pieces", x + 264, y, 60); tPieces = Text_(p, x + 264, y + 15, 100, "count of pieces; blank = 4");
            y += 46;

            Field(p, "combine", x, y, 120);
            string[] combos = { "sum", "concat", "min", "max", "count", "first" };
            combineBtns = new Button[combos.Length]; int bx = x;
            for (int i = 0; i < combos.Length; i++)
            {
                string name = combos[i]; int bw = 22 + name.Length * 9;
                var b = Btn(p, name, bx, y + 14, bw, 26, false);
                b.Click += delegate { SelectCombine(name); };
                combineBtns[i] = b; bx += bw + 6;
            }
            y += 48;

            Field(p, "across", x, y, 90); tAcross = Text_(p, x, y + 15, 90, "quorum: each piece on N machines, majority");
            Field(p, "budget", x + 106, y, 90); tBudget = Text_(p, x + 106, y + 15, 90, "seconds per worker");
            Field(p, "input", x + 212, y, 90); tInput = Text_(p, x + 212, y + 15, lw - 212, "petname of an object on the node");
            y += 46;

            planLbl = new Label(); planLbl.AutoSize = false; planLbl.Location = new Point(x, y); planLbl.Size = new Size(lw - 130, 30); planLbl.Font = Look.Small; planLbl.ForeColor = Look.Dim; p.Controls.Add(planLbl);
            gaugeLbl = new Label(); gaugeLbl.AutoSize = false; gaugeLbl.Location = new Point(x + lw - 126, y); gaugeLbl.Size = new Size(126, 15); gaugeLbl.Font = Look.Small; gaugeLbl.ForeColor = Look.Dim; gaugeLbl.TextAlign = ContentAlignment.TopRight; p.Controls.Add(gaugeLbl);
            y += 34;

            Field(p, "editor  --  the payload sent to the machines", x, y, lw);
            tEditor = new TextBox();
            tEditor.Multiline = true; tEditor.AcceptsTab = true; tEditor.ScrollBars = ScrollBars.Both;
            tEditor.BorderStyle = BorderStyle.None; tEditor.BackColor = Look.Panel; tEditor.ForeColor = Look.Ink;
            tEditor.Font = Look.Mono; tEditor.WordWrap = false;
            tEditor.Location = new Point(x + 2, y + 17); tEditor.Size = new Size(lw - 4, p.Height - (y + 17) - 54);
            var ep = new Panel(); ep.BackColor = Look.Panel; ep.Location = new Point(x, y + 15); ep.Size = new Size(lw, p.Height - (y + 15) - 50); ep.Controls.Add(tEditor);
            p.Controls.Add(ep); bordered.Add(ep); Dark(tEditor);
            tEditor.TextChanged += delegate { RefreshPreview(); };

            var tl = Field(p, "templates", x, p.Height - 34, 80); tl.ForeColor = Look.Faint;
            Btn(p, "sum of a range", x + 84, p.Height - 38, 156, 26, false).Click += delegate { LoadTemplate("sumrange"); };
            Btn(p, "count matches", x + 248, p.Height - 38, 150, 26, false).Click += delegate { LoadTemplate("count"); };

            /* right column: send + result + output */
            int rx = 512, rw = p.Width - rx - 22;
            Section(p, "run", rx, 16, rw);
            Field(p, "wait for the folded result (seconds)", rx, 46, rw);
            tWait = Text_(p, rx, 61, 90, "seconds to wait for the answer; 0 = fire and forget");
            sendBtn = Btn(p, "send to target", rx + 106, 59, 200, 30, true); sendBtn.Click += delegate { DoSend(); };
            Btn(p, "save package", rx + 316, 59, rw - 316, 30, false).Click += delegate { DoSave(); };

            Field(p, "result", rx, 100, rw - 84);
            tResult = new TextBox(); tResult.BorderStyle = BorderStyle.None; tResult.BackColor = Look.Field; tResult.ForeColor = Look.Accent; tResult.Font = Look.Res; tResult.ReadOnly = true;
            tResult.Location = new Point(rx + 3, 119); tResult.Size = new Size(rw - 90, 24);
            var rp = new Panel(); rp.BackColor = Look.Field; rp.Location = new Point(rx, 117); rp.Size = new Size(rw - 84, 30); rp.Controls.Add(tResult); p.Controls.Add(rp); bordered.Add(rp);
            Btn(p, "copy", rx + rw - 78, 117, 78, 30, false).Click += delegate { try { if (tResult.Text.Length > 0) Clipboard.SetText(tResult.Text); } catch { } };

            Field(p, "output", rx, 158, rw);
            var op = new Panel(); op.BackColor = Look.Panel; op.Location = new Point(rx, 173); op.Size = new Size(rw, p.Height - 173 - 16);
            tOut = new TextPane(); tOut.Location = new Point(1, 1); tOut.Size = new Size(op.Width - 2, op.Height - 2);
            op.Controls.Add(tOut); p.Controls.Add(op); bordered.Add(op);
            tOut.SetText("idle.");

            SelectCombine("sum");
            if (demo) LoadTemplate("sumrange");
        }

        Toggle MkToggle(Control host, string text, int x, int y, string tip)
        {
            var t = new Toggle(text); t.Location = new Point(x, y); t.Changed += delegate { RefreshPreview(); };
            if (tip != null) tips.SetToolTip(t, tip); host.Controls.Add(t); return t;
        }
        void SelectCombine(string name)
        {
            combineSel = name; string[] combos = { "sum", "concat", "min", "max", "count", "first" };
            for (int i = 0; i < combineBtns.Length; i++)
            {
                bool on = combos[i] == name;
                combineBtns[i].ForeColor = on ? Look.Accent : Look.Dim;
                combineBtns[i].FlatAppearance.BorderColor = on ? Look.Accent : Look.Edge;
            }
            RefreshPreview();
        }
        static int PInt(string t) { int v; return int.TryParse((t ?? "").Trim(), out v) ? v : 0; }
        static long PLong(string t) { long v; return long.TryParse((t ?? "").Trim(), out v) ? v : 0; }

        Spec Gather(out string err)
        {
            err = null; var s = new Spec();
            s.Recipe = kRecipe.Checked; s.Name = tName.Text.Trim(); s.InputName = tInput.Text.Trim(); s.Combine = combineSel;
            s.Across = PInt(tAcross.Text); s.Budget = PInt(tBudget.Text); s.Pieces = PInt(tPieces.Text);
            int w; s.Wait = int.TryParse((tWait.Text ?? "").Trim(), out w) ? Math.Max(0, w) : 6;
            if (kSplit.Checked) { s.HasSplit = true; s.SplitLo = PLong(tLo.Text); s.SplitHi = PLong(tHi.Text); if (s.SplitHi < s.SplitLo) { err = "hi < lo."; return null; } }
            return s;
        }
        byte[] Payload() { string t = tEditor.Text; return string.IsNullOrEmpty(t) ? new byte[0] : new UTF8Encoding(false).GetBytes(t); }
        void RefreshPreview()
        {
            if (loading || tEditor == null) return;
            string err; var s = Gather(out err);
            if (s == null) { planLbl.ForeColor = Look.WarnC; planLbl.Text = err; return; }
            byte[] pkg = Core.Package(s, Payload()); int bytes = pkg.Length;
            gaugeLbl.ForeColor = bytes > Core.WireMax ? Look.WarnC : Look.Dim; gaugeLbl.Text = bytes + " / " + Core.WireMax + " B";
            int pieces = s.HasSplit ? (s.Pieces > 0 ? s.Pieces : 4) : 1; int quorum = s.Across > 0 ? s.Across : 1; int slots = pieces * quorum;
            var plan = new StringBuilder(); plan.Append(pieces).Append(pieces == 1 ? " piece" : " pieces");
            if (quorum > 1) plan.Append(" x ").Append(quorum).Append(" = ").Append(slots).Append(" slots");
            plan.Append("  \u00b7  combine ").Append(s.Combine);
            var warn = new StringBuilder();
            if (bytes > Core.WireMax) warn.Append("payload > ").Append(Core.WireMax).Append(" B.  ");
            if (slots > Core.SlotMax) warn.Append(slots).Append(" slots > ").Append(Core.SlotMax).Append(".  ");
            if (warn.Length > 0) { planLbl.ForeColor = Look.WarnC; planLbl.Text = plan.ToString() + "\n" + warn.ToString(); }
            else { planLbl.ForeColor = Look.Dim; planLbl.Text = plan.ToString(); }
        }
        void Out(string line)
        {
            if (tOut.InvokeRequired) { tOut.BeginInvoke((MethodInvoker)delegate { Out(line); }); return; }
            tOut.Append(line);
        }
        void LoadTemplate(string which)
        {
            loading = true;
            string[] body = which == "sumrange"
                ? new string[] { "long main(long console, long inbox)", "{", "    long buf[16];", "    long lo, hi, s, i;", "    syscall(3, inbox, buf, 0, 0, 0);", "    lo = buf[2];", "    hi = buf[3];", "    s = 0;", "    for (i = lo; i <= hi; i = i + 1)", "        s = s + i;", "    syscall(2, console, 0x54584554, s, 0, 0);", "    return 0;", "}" }
                : new string[] { "long main(long console, long inbox)", "{", "    long buf[16];", "    long lo, hi, k, i;", "    syscall(3, inbox, buf, 0, 0, 0);", "    lo = buf[2];", "    hi = buf[3];", "    k = 0;", "    for (i = lo; i <= hi; i = i + 1)", "        if ((i & 7) == 0) k = k + 1;", "    syscall(2, console, 0x54584554, k, 0, 0);", "    return 0;", "}" };
            tEditor.Text = string.Join("\r\n", body);
            kRecipe.Checked = false; tName.Text = which; kSplit.Checked = true; tLo.Text = "1"; tHi.Text = "1000000"; tPieces.Text = "8"; tBudget.Text = "15";
            SelectCombine(which == "sumrange" ? "sum" : "sum");
            loading = false; RefreshPreview();
        }
        void DoSend()
        {
            if (target == null) { Say("no target node."); return; }
            string err; var s = Gather(out err); if (s == null) { Out(err); return; }
            byte[] payload = Payload(); if (payload.Length == 0) { Out("no payload (type in the editor)."); return; }
            byte[] pkg = Core.Package(s, payload);
            if (pkg.Length > Core.WireMax) { Out("payload " + pkg.Length + " B > " + Core.WireMax + " B; not sent."); return; }
            tResult.Text = ""; sendBtn.Enabled = false;
            Out(""); Out("--- send " + pkg.Length + " B -> " + target + " ---");
            var n = target; int wait = s.Wait;
            Bg(delegate
            {
                string e2; var c = Ctl.Open(SpecOf(n), out e2);
                if (c == null) { Out("control: " + e2); UI(delegate { sendBtn.Enabled = true; }); return; }
                var fs = c.Req("submit", pkg);
                if (fs == null || fs.Kind != 2) { Out("submit: " + (fs != null ? Encoding.UTF8.GetString(fs.Body) : "no response")); c.Close(); UI(delegate { sendBtn.Enabled = true; }); return; }
                string handle = Ctl.KV(fs.Body, "handle"); Out("submitted; handle " + handle);
                if (wait > 0 && handle != null)
                {
                    DateTime dl = DateTime.UtcNow.AddSeconds(wait); byte[] hb = Encoding.ASCII.GetBytes(handle);
                    string state = "pending", result = null;
                    while (DateTime.UtcNow < dl)
                    {
                        Thread.Sleep(400);
                        var fr = c.Req("result", hb);
                        if (fr != null && fr.Kind == 2) { state = Ctl.KV(fr.Body, "state") ?? "pending"; result = Ctl.KV(fr.Body, "result"); if (state == "done") break; }
                    }
                    Out("= " + (result ?? "(" + state + ")"));
                    if (result != null) UI(delegate { tResult.Text = result; });
                }
                c.Close();
                UI(delegate { sendBtn.Enabled = true; Say("sent."); });
            });
        }
        void DoSave()
        {
            string err; var s = Gather(out err); if (s == null) { Out(err); return; }
            byte[] payload = Payload(); if (payload.Length == 0) { Out("no payload."); return; }
            byte[] pkg = Core.Package(s, payload);
            var d = new SaveFileDialog(); d.Filter = "task package|*.ebtask|all|*.*"; d.FileName = (string.IsNullOrEmpty(s.Name) ? "task" : s.Name) + ".ebtask";
            if (d.ShowDialog() == DialogResult.OK) { File.WriteAllBytes(d.FileName, pkg); Out("wrote " + d.FileName + " (" + pkg.Length + " bytes)"); }
        }

        /* ================= JOBS ========================================= */

        Grid jbGrid, evGrid; List<string[]> events = new List<string[]>();
        Toggle jbAuto, evWatch; System.Windows.Forms.Timer jbTimer; Ctl watchCtl; Thread watchThread; bool watching;

        void BuildJobs()
        {
            var p = View("jobs");
            Section(p, "jobs in flight", 22, 16, p.Width - 44);
            Btn(p, "refresh", 22, 44, 100, 26, false).Click += delegate { RefreshJobs(); };
            jbAuto = new Toggle("auto"); jbAuto.Location = new Point(134, 48); p.Controls.Add(jbAuto);
            jbAuto.Changed += delegate { if (jbAuto.Checked) jbTimer.Start(); else jbTimer.Stop(); };

            jbGrid = MakeGrid(p, 22, 80, p.Width - 44, 200);
            jbGrid.Columns(new Grid.Column[] { new Grid.Column("no", 50), new Grid.Column("name", 160), new Grid.Column("state", 96), new Grid.Column("pieces", 84), new Grid.Column("parts", 80), new Grid.Column("done", 74), new Grid.Column("quorum", 92), new Grid.Column("combine", 100) });
            jbGrid.Tint = delegate(int r, int c, string t) { return (c == 2) ? (t == "run" ? Look.Accent : t == "scan" ? Look.WarnC : Look.Ink) : Look.Ink; };

            Section(p, "event stream", 22, 300, p.Width - 44);
            evWatch = new Toggle("watch (live push)"); evWatch.Location = new Point(22, 326); p.Controls.Add(evWatch);
            evWatch.Changed += delegate { if (evWatch.Checked) StartWatch(); else StopWatch(); };
            Btn(p, "clear", 220, 322, 80, 26, false).Click += delegate { events.Clear(); evGrid.SetRows(new List<string[]>()); };

            evGrid = MakeGrid(p, 22, 358, p.Width - 44, p.Height - 358 - 16);
            evGrid.Columns(new Grid.Column[] { new Grid.Column("time", 90), new Grid.Column("job", 50), new Grid.Column("kind", 90), new Grid.Column("detail", 400) });
            evGrid.Tint = delegate(int r, int c, string t) { return (c == 2) ? (t == "done" ? Look.Accent : t == "failed" ? Look.WarnC : Look.Ink) : Look.Ink; };

            jbTimer = new System.Windows.Forms.Timer(); jbTimer.Interval = 4000; jbTimer.Tick += delegate { RefreshJobs(); };
            this.FormClosing += delegate { jbTimer.Stop(); StopWatch(); };
            if (demo) SeedJobs();
        }

        void RefreshJobs()
        {
            if (demo) return;
            var n = target; if (n == null) { Say("no target node."); return; }
            Bg(delegate
            {
                string err; var f = OneShot(n, "jobs", null, out err);
                var rows = new List<string[]>();
                if (f != null && f.Kind == 2)
                    foreach (var line in Encoding.UTF8.GetString(f.Body).Split('\n'))
                    {
                        if (line.Length == 0 || line.StartsWith("count=")) continue;
                        var d = Parse(line);
                        rows.Add(new string[] { Get(d, "no"), Get(d, "name"), Get(d, "state"), Get(d, "pieces"), Get(d, "parts"), Get(d, "done"), Get(d, "quorum"), Get(d, "combine") });
                    }
                UI(delegate { jbGrid.SetRows(rows); Say(f != null ? "jobs: " + rows.Count + " in flight." : "jobs: " + (err ?? "no answer")); });
            });
        }
        void StartWatch()
        {
            if (demo || watching) return;
            var n = target; if (n == null) { Say("no target node."); evWatch.Checked = false; return; }
            watching = true; Say("subscribed to " + n + ".");
            watchThread = new Thread(delegate()
            {
                string err; watchCtl = Ctl.Open(SpecOf(n), out err);
                if (watchCtl == null) { UI(delegate { Say("watch: " + err); evWatch.Checked = false; watching = false; }); return; }
                var sub = watchCtl.Req("subscribe", null);
                if (sub == null || sub.Kind != 2) { UI(delegate { Say("watch: subscribe failed"); evWatch.Checked = false; watching = false; }); watchCtl.Close(); return; }
                while (watching)
                {
                    var f = watchCtl.ReadFrame();
                    if (f == null) break;
                    if (f.Kind == 3)
                    {
                        var d = Parse(Encoding.UTF8.GetString(f.Body));
                        string detail = Get(d, "text"); if (detail.Length == 0) detail = "seq " + Get(d, "seq");
                        AddEvent(new string[] { DateTime.Now.ToString("HH:mm:ss"), Get(d, "no"), Get(d, "kind"), detail });
                    }
                }
                watching = false;
            });
            watchThread.IsBackground = true; watchThread.Start();
        }
        void StopWatch()
        {
            watching = false;
            try { if (watchCtl != null) watchCtl.Close(); } catch { }
            watchCtl = null;
        }
        void AddEvent(string[] row)
        {
            UI(delegate
            {
                events.Insert(0, row); if (events.Count > 200) events.RemoveAt(events.Count - 1);
                evGrid.SetRows(new List<string[]>(events));
            });
        }
        void SeedJobs()
        {
            jbGrid.SetRows(new List<string[]> {
                new string[]{"7","maxdemo","run","2","2","1","0","max"},
                new string[]{"8","sumrange","scan","8","8","0","0","sum"},
            });
            events = new List<string[]> {
                new string[]{"14:22:07","8","queued","seq 5"},
                new string[]{"14:22:05","7","done","= 100"},
                new string[]{"14:22:01","7","queued","seq 3"},
                new string[]{"14:21:40","6","failed","no peer is named in the settings"},
            };
            evGrid.SetRows(new List<string[]>(events));
        }

        /* ================= LOGS ========================================= */

        TextPane lgText; Toggle lgAuto; TextBox lgLines; System.Windows.Forms.Timer lgTimer;

        void BuildLogs()
        {
            var p = View("logs");
            Section(p, "log", 22, 16, p.Width - 44);
            Btn(p, "refresh", 22, 44, 100, 26, false).Click += delegate { RefreshLog(); };
            lgAuto = new Toggle("auto"); lgAuto.Location = new Point(134, 48); p.Controls.Add(lgAuto);
            lgAuto.Changed += delegate { if (lgAuto.Checked) lgTimer.Start(); else lgTimer.Stop(); };
            Field(p, "lines", p.Width - 160, 30, 40); lgLines = Text_(p, p.Width - 118, 44, 96, "how many journal lines"); lgLines.Text = "40";

            var box = new Panel(); box.BackColor = Look.Panel; box.Location = new Point(22, 80); box.Size = new Size(p.Width - 44, p.Height - 80 - 14);
            lgText = new TextPane(); lgText.Location = new Point(1, 1); lgText.Size = new Size(box.Width - 2, box.Height - 2);
            box.Controls.Add(lgText); p.Controls.Add(box); bordered.Add(box);

            lgTimer = new System.Windows.Forms.Timer(); lgTimer.Interval = 4000; lgTimer.Tick += delegate { RefreshLog(); };
            this.FormClosing += delegate { lgTimer.Stop(); };
            if (demo) SeedLog();
        }
        void RefreshLog()
        {
            if (demo) return;
            var n = target; if (n == null) { Say("no target node."); return; }
            int lines = PInt(lgLines.Text); byte[] body = lines > 0 ? Encoding.ASCII.GetBytes(lines.ToString()) : null;
            Bg(delegate
            {
                string err; var f = OneShot(n, "log", body, out err);
                string txt = (f != null && f.Kind == 2) ? Encoding.UTF8.GetString(f.Body) : ("(" + (err ?? "no answer") + ")");
                UI(delegate { lgText.SetText(txt); Say("log from " + n + "."); });
            });
        }
        void SeedLog()
        {
            lgText.SetText(string.Join("\n", new string[] {
                "   3s  ssh: the door's key is SHA256:QGFaYCgd26kmZCPsexPFkwc",
                "   3s  system: started fresh",
                "   5s  system: the clock was set from the net",
                "  31s  ssh: someone logged in",
                "  71s  pipe: job 1 queued, 39 bytes, parts 2",
                "  76s  pipe: job 1 answers: 100 (2 parts by alpha, gamma)",
                " 132s  pipe: node delta went quiet",
            }));
        }

        /* ================= CLUSTER ====================================== */

        Grid clGrid; Panel clTopo; List<string[]> clRows = new List<string[]>();

        void BuildCluster()
        {
            var p = View("cluster");
            Section(p, "cluster", 22, 16, p.Width - 44);
            Btn(p, "refresh", 22, 44, 100, 26, false).Click += delegate { RefreshCluster(); };
            Field(p, "gateway = the target node; a scan is poked, then the heard peers are read", 134, 50, p.Width - 160);

            clGrid = MakeGrid(p, 22, 80, p.Width - 44, 230);
            clGrid.Columns(new Grid.Column[] { new Grid.Column("node", 200), new Grid.Column("version", 200), new Grid.Column("jobs", 70), new Grid.Column("works", 80), new Grid.Column("seen", 100) });
            clGrid.Tint = delegate(int r, int c, string t) { if (c == 0 && r == 0) return Look.Accent; if (c == 3) return t == "yes" ? Look.Accent : Look.Dim; return Look.Ink; };

            Section(p, "topology", 22, 326, p.Width - 44);
            clTopo = new Panel(); clTopo.Location = new Point(22, 352); clTopo.Size = new Size(p.Width - 44, p.Height - 352 - 16); clTopo.BackColor = Look.Panel;
            clTopo.Paint += delegate(object o, PaintEventArgs e) { PaintTopo(e.Graphics, clTopo.ClientSize); };
            p.Controls.Add(clTopo); bordered.Add(clTopo);

            if (demo) SeedCluster();
        }
        void RefreshCluster()
        {
            if (demo) return;
            var n = target; if (n == null) { Say("no target node."); return; }
            Say("aggregating the cluster from " + n + " ...");
            Bg(delegate
            {
                string err; var c = Ctl.Open(SpecOf(n), out err);
                if (c == null) { UI(delegate { Say("cluster: " + err); }); return; }
                c.Req("cluster", null); Thread.Sleep(4200);
                var f = c.Req("cluster", null); c.Close();
                var rows = new List<string[]>();
                if (f != null && f.Kind == 2)
                    foreach (var line in Encoding.UTF8.GetString(f.Body).Split('\n'))
                    {
                        if (line.Length == 0 || line.StartsWith("peers=")) continue;
                        if (line.StartsWith("self "))
                        { var d = Parse(line.Substring(5)); rows.Insert(0, new string[] { (n.Name.Length > 0 ? n.Name : "self") + " *", Get(d, "version"), Get(d, "jobs"), "-", "self" }); }
                        else if (line.StartsWith("peer "))
                        { var d = Parse(line.Substring(5)); rows.Add(new string[] { Get(d, "name").Length > 0 ? Get(d, "name") : Get(d, "ip"), Get(d, "version"), Get(d, "jobs"), Get(d, "works"), Get(d, "seen_s") + "s" }); }
                    }
                UI(delegate { clRows = rows; clGrid.SetRows(rows); clTopo.Invalidate(); Say("cluster: " + Math.Max(0, rows.Count - 1) + " peer(s)."); });
            });
        }
        void PaintTopo(Graphics g, Size sz)
        {
            g.Clear(Look.Panel); g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
            g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
            if (clRows.Count == 0) { using (var b = new SolidBrush(Look.Faint)) g.DrawString("(refresh to draw the mesh)", Look.Mono, b, 12, 12); return; }
            int cx = sz.Width / 2, cy = sz.Height / 2; int R = Math.Min(cx, cy) - 46;
            int peers = clRows.Count - 1;
            using (var pen = new Pen(Look.Edge))
            for (int i = 1; i < clRows.Count; i++)
            {
                double ang = -Math.PI / 2 + 2 * Math.PI * (i - 1) / Math.Max(1, peers);
                int px = cx + (int)(R * Math.Cos(ang)), py = cy + (int)(R * Math.Sin(ang));
                g.DrawLine(pen, cx, cy, px, py);
                Node(g, px, py, clRows[i][0], clRows[i][3] == "yes", false);
            }
            Node(g, cx, cy, clRows[0][0], false, true);
        }
        void Node(Graphics g, int x, int y, string name, bool works, bool self)
        {
            int r = 9; Color c = self ? Look.Accent : (works ? Look.Ink : Look.Dim);
            using (var b = new SolidBrush(Look.Ground)) g.FillEllipse(b, x - r, y - r, 2 * r, 2 * r);
            using (var pen = new Pen(c, self ? 2f : 1f)) g.DrawEllipse(pen, x - r, y - r, 2 * r, 2 * r);
            using (var tb = new SolidBrush(c)) { var sz = g.MeasureString(name, Look.Small); g.DrawString(name, Look.Small, tb, x - sz.Width / 2, y + r + 2); }
        }
        void SeedCluster()
        {
            clRows = new List<string[]> {
                new string[]{"beta *","0.9.6","1","self","self"},
                new string[]{"alpha","0.9.6","0","yes","2s"},
                new string[]{"gamma","0.9.6","0","yes","3s"},
                new string[]{"delta","0.9.5","0","no","48s"},
            };
            clGrid.SetRows(clRows);
        }

        /* ================= SETTINGS ===================================== */

        void BuildSettings()
        {
            var p = View("settings");
            Section(p, "settings", 22, 16, 560);
            Field(p, "default target node (host)", 22, 46, 400);
            var dt = Text_(p, 22, 61, 400, "the node the views act on when Gate opens"); dt.Text = target != null ? target.Host : "";
            dt.TextChanged += delegate { var n = ByHost(dt.Text.Trim()); if (n != null) SetTarget(n); };

            Section(p, "the machine interface", 22, 110, 560);
            var m = new Label(); m.AutoSize = false; m.Location = new Point(22, 138); m.Size = new Size(p.Width - 44, 150); m.Font = Look.Mono; m.ForeColor = Look.Dim;
            m.Text = "the same views drive a program from the command line:\n\n"
                   + "  EreBUS-Gate.exe api status  --node user@host\n"
                   + "  EreBUS-Gate.exe api jobs    --node user@host\n"
                   + "  EreBUS-Gate.exe api submit  --source prog.c --node user@host --split 1..1000000 --pieces 8 --combine max --wait 6\n"
                   + "  EreBUS-Gate.exe api watch   --node user@host --for 60\n"
                   + "  EreBUS-Gate.exe api cluster --node user@host\n\n"
                   + "'api schema' lists every verb. json on stdout, one object per call.";
            p.Controls.Add(m);

            Section(p, "about", 22, 300, 560);
            var a = new Label(); a.AutoSize = false; a.Location = new Point(22, 328); a.Size = new Size(p.Width - 44, 120); a.Font = Look.Mono; a.ForeColor = Look.Ink;
            a.Text = "EreBUS Gate " + Ver.V + "\n\n"
                   + "a front-end and machine interface for an EreBUS far-work cluster.\n"
                   + "it speaks the node's control channel over ssh (EreBUS 0.9.6).\n"
                   + "the node's door must hold your ssh key; a payload is at most 1 KiB and\n"
                   + "no foreign binaries run -- the node compiles the c or runs the recipe.";
            p.Controls.Add(a);
        }

        /* ---- seed / persist ------------------------------------------- */

        void SeedDemo()
        {
            nodes.Add(new NodeEntry() { Name = "alpha", Host = "someone@10.11.11.20", Port = 22 });
            nodes.Add(new NodeEntry() { Name = "beta", Host = "someone@10.11.11.21", Port = 22 });
            nodes.Add(new NodeEntry() { Name = "gamma", Host = "someone@10.11.11.22", Port = 22 });
            nodes.Add(new NodeEntry() { Name = "delta", Host = "someone@10.11.11.23", Port = 22 });
        }
        void Persist()
        {
            if (target != null) last["target"] = target.Host;
            if (!demo) Store.Save(nodes, last);
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
#if CONSOLE
            /* EreBUS-Gate-api.exe: the same program built for a console, so
             * that a shell can redirect or pipe what it prints. Windows hands
             * a window program no standard handles, so the window build's
             * "api status > file" and "api watch | jq" reach nobody, however
             * the console is attached; a program that spawns it with pipes of
             * its own gets the lines either way. The window is the other
             * build; this one never opens it. */
            if (args.Length > 0 && args[0] == "api") return Api(args);
            if (Has(args, "--version")) { Console.WriteLine("EreBUS Gate " + Ver.V); return 0; }
            if (Has(args, "--shot")) return Shot(Arg(args, "--shot"), Arg(args, "--view"));
            if (Has(args, "--headless")) return Headless(args);
            Console.Error.WriteLine("EreBUS Gate " + Ver.V + ", the machine interface: api <verb>, --headless, --shot.");
            Console.Error.WriteLine("'api schema' lists the verbs; the window is EreBUS-Gate.exe.");
            return 2;
#else
            if (args.Length > 0) AttachConsole(-1);
            if (args.Length > 0 && args[0] == "api") return Api(args);
            if (Has(args, "--version")) { Console.WriteLine("EreBUS Gate " + Ver.V); return 0; }
            if (Has(args, "--shot")) return Shot(Arg(args, "--shot"), Arg(args, "--view"));
            if (Has(args, "--headless")) return Headless(args);
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new GateForm());
            return 0;
#endif
        }

        static bool Has(string[] a, string k) { foreach (var x in a) if (x == k) return true; return false; }
        static string Arg(string[] a, string k) { for (int i = 0; i < a.Length - 1; i++) if (a[i] == k) return a[i + 1]; return null; }

        static int Shot(string path, string view)
        {
            if (string.IsNullOrEmpty(path)) { Console.Error.WriteLine("--shot needs a file"); return 2; }
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            var f = new GateForm(true);
            f.StartPosition = FormStartPosition.Manual; f.Location = new Point(-4000, -4000);
            f.Show(); Application.DoEvents();
            if (!string.IsNullOrEmpty(view)) { f.Show(view); Application.DoEvents(); }
            System.Threading.Thread.Sleep(120); Application.DoEvents();
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
