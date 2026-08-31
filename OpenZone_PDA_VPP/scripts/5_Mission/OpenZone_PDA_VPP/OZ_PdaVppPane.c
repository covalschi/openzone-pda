// Панель «PDA» в адмiнському вiкнi OpenZone: форма Tuning.json полями,
// праворуч -- мiнi-вкладки HARDWARE (форма модулiв i носiїв) та PROFILES
// (сирий JSON). Чiпляється вкладкою до вiкна ядра через modded class --
// «субмод субмода»: ядро про КПК не знає, КПК доклада свою вкладку сам.
//
// Гарди: NO_GUI -- сервер компiлює Mission без UI; AVPPAdminTools i
// OpenZone_VPP -- iмена класiв CfgMods (їх авто-дефайнить рушiй).

#ifdef AVPPAdminTools
#ifdef OpenZone_VPP
#ifndef NO_GUI

modded class OZ_VppAdminMenu
{
    private ref OZ_PdaTuning m_PdaTun;
    private ref OZ_PdaHardwareConfig m_HwCfg;

    // Рядок списку залiза -> (вид, iндекс у своєму масивi).
    private ref array<string> m_HwRowKind;
    private ref array<int>    m_HwRowIdx;
    private string m_HwPickedKind = "";
    private int    m_HwPickedIdx = -1;
    private bool   m_HwNewMode = false;
    private bool   m_HwWritable = true;
    private bool   m_HwDelArmed = false;

    override void OnCreate(Widget RootW)
    {
        super.OnCreate(RootW);

        if (!M_SUB_WIDGET)
            return;

        m_HwRowKind = new array<string>();
        m_HwRowIdx  = new array<int>();

        Widget pane = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA_VPP/gui/layouts/oz_pda_vpp_pane.layout", M_SUB_WIDGET);
        if (!pane)
        {
            OZ_Log.Error("pda vpp pane: layout failed to load");
            return;
        }

        RegisterPane("pda", "PDA", pane);
    }

    override void OnPaneShown(string id)
    {
        super.OnPaneShown(id);

        if (id == "pda")
        {
            AskCfg("Tuning");
            AskCfg("Hardware");
        }
    }

    override void OnCfgText(string name, string body)
    {
        if (name == "Tuning")
        {
            OZ_PdaTuning t;
            string err;
            if (JsonFileLoader<OZ_PdaTuning>.LoadData(body, t, err) && t)
            {
                m_PdaTun = t;
                FillTuningForm();
            }
            else
                Hint("Tuning.json does not parse: " + err);
            return;
        }

        if (name == "Hardware")
        {
            OZ_PdaHardwareConfig hc;
            string herr;
            if (JsonFileLoader<OZ_PdaHardwareConfig>.LoadData(body, hc, herr) && hc)
            {
                m_HwCfg = hc;
                RebuildHwList();
            }
            else
            {
                Hint("Hardware.json does not parse: " + herr);
            }
            return;
        }

        if (name == "Profiles")
        {
            MultilineEditBoxWidget ed = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("PdaProfEdit"));
            if (ed)
                ed.SetText(body);
            Hint("Profiles loaded");
            return;
        }

        super.OnCfgText(name, body);
    }

    override void OnCfgApplied()
    {
        super.OnCfgApplied();
        if (CurrentPane() == "pda")
        {
            AskCfg("Tuning");
            AskCfg("Hardware");
        }
    }

    // ---------------------------------------------------------- тюнiнг

    private void FillTuningForm()
    {
        if (!m_PdaTun)
            return;

        SetEdit("Tun_PinMaxFails",         m_PdaTun.PinMaxFails.ToString());
        SetEdit("Tun_PinLockoutSeconds",   m_PdaTun.PinLockoutSeconds.ToString());
        SetEdit("Tun_ChatMsgMaxBytes",     m_PdaTun.ChatMsgMaxBytes.ToString());
        SetEdit("Tun_ChatTitleMaxBytes",   m_PdaTun.ChatTitleMaxBytes.ToString());
        SetEdit("Tun_ChatDescMaxBytes",    m_PdaTun.ChatDescMaxBytes.ToString());
        SetEdit("Tun_ChatHistoryOpen",     m_PdaTun.ChatHistoryOpen.ToString());
        SetEdit("Tun_ChatHistoryPage",     m_PdaTun.ChatHistoryPage.ToString());
        SetEdit("Tun_ChatGroupMax",        m_PdaTun.ChatGroupMax.ToString());
        SetEdit("Tun_NotesMaxFallback",    m_PdaTun.NotesMaxFallback.ToString());
        SetEdit("Tun_NoteTitleMaxBytes",   m_PdaTun.NoteTitleMaxBytes.ToString());
        SetEdit("Tun_NoteBodyMaxBytes",    m_PdaTun.NoteBodyMaxBytes.ToString());
        SetEdit("Tun_MarkerNameMaxBytes",  m_PdaTun.MarkerNameMaxBytes.ToString());
        SetEdit("Tun_MarkerDescMaxBytes",  m_PdaTun.MarkerDescMaxBytes.ToString());
        SetEdit("Tun_FriendReachMeters",   m_PdaTun.FriendReachMeters.ToString());
        SetEdit("Tun_SwapOfferTtlSeconds", m_PdaTun.SwapOfferTtlSeconds.ToString());
        SetEdit("Tun_ToastSeconds",        m_PdaTun.ToastSeconds.ToString());
        SetEdit("Tun_RouteAdvanceMeters",  m_PdaTun.RouteAdvanceMeters.ToString());
        SetEdit("Tun_BeaconPushSeconds",   m_PdaTun.BeaconPushSeconds.ToString());
    }

    private void SaveTuningForm()
    {
        if (!m_PdaTun)
        {
            Hint("Tuning is not loaded yet");
            return;
        }

        // Правимо ЗАВАНТАЖЕНИЙ об'єкт: Version i майбутнi поля, яких форма
        // не знає, переживають збереження недоторканими.
        m_PdaTun.PinMaxFails         = GetEdit("Tun_PinMaxFails").ToInt();
        m_PdaTun.PinLockoutSeconds   = GetEdit("Tun_PinLockoutSeconds").ToInt();
        m_PdaTun.ChatMsgMaxBytes     = GetEdit("Tun_ChatMsgMaxBytes").ToInt();
        m_PdaTun.ChatTitleMaxBytes   = GetEdit("Tun_ChatTitleMaxBytes").ToInt();
        m_PdaTun.ChatDescMaxBytes    = GetEdit("Tun_ChatDescMaxBytes").ToInt();
        m_PdaTun.ChatHistoryOpen     = GetEdit("Tun_ChatHistoryOpen").ToInt();
        m_PdaTun.ChatHistoryPage     = GetEdit("Tun_ChatHistoryPage").ToInt();
        m_PdaTun.ChatGroupMax        = GetEdit("Tun_ChatGroupMax").ToInt();
        m_PdaTun.NotesMaxFallback    = GetEdit("Tun_NotesMaxFallback").ToInt();
        m_PdaTun.NoteTitleMaxBytes   = GetEdit("Tun_NoteTitleMaxBytes").ToInt();
        m_PdaTun.NoteBodyMaxBytes    = GetEdit("Tun_NoteBodyMaxBytes").ToInt();
        m_PdaTun.MarkerNameMaxBytes  = GetEdit("Tun_MarkerNameMaxBytes").ToInt();
        m_PdaTun.MarkerDescMaxBytes  = GetEdit("Tun_MarkerDescMaxBytes").ToInt();
        m_PdaTun.FriendReachMeters   = GetEdit("Tun_FriendReachMeters").ToInt();
        m_PdaTun.SwapOfferTtlSeconds = GetEdit("Tun_SwapOfferTtlSeconds").ToInt();
        m_PdaTun.ToastSeconds        = GetEdit("Tun_ToastSeconds").ToInt();
        m_PdaTun.RouteAdvanceMeters  = GetEdit("Tun_RouteAdvanceMeters").ToInt();
        m_PdaTun.BeaconPushSeconds   = GetEdit("Tun_BeaconPushSeconds").ToInt();

        string body;
        string err;
        if (!JsonFileLoader<OZ_PdaTuning>.MakeData(m_PdaTun, body, err, false))
        {
            Hint("cannot serialise");
            return;
        }
        SendCfg("Tuning", body);
    }

    // ---------------------------------------------------------- залiзо

    private void RebuildHwList()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("HwList"));
        if (!lb || !m_HwCfg)
            return;

        lb.ClearItems();
        m_HwRowKind.Clear();
        m_HwRowIdx.Clear();

        if (m_HwCfg.Modules)
        {
            for (int i = 0; i < m_HwCfg.Modules.Count(); i++)
            {
                lb.AddItem("M  " + m_HwCfg.Modules[i].ClassName, NULL, 0);
                m_HwRowKind.Insert("module");
                m_HwRowIdx.Insert(i);
            }
        }

        if (m_HwCfg.Carriers)
        {
            for (int c = 0; c < m_HwCfg.Carriers.Count(); c++)
            {
                lb.AddItem("C  " + m_HwCfg.Carriers[c].ClassName, NULL, 0);
                m_HwRowKind.Insert("carrier");
                m_HwRowIdx.Insert(c);
            }
        }
    }

    private void FillHwForm(string kind, int idx)
    {
        m_HwPickedKind = kind;
        m_HwPickedIdx  = idx;
        m_HwNewMode    = false;
        m_HwDelArmed   = false;

        if (kind == "module")
        {
            OZ_ModuleSpec ms = m_HwCfg.Modules[idx];
            SetEdit("HwClass", ms.ClassName);
            SetEdit("HwName",  ms.DisplayName);
            SetEdit("HwKind",  ms.Kind);
            SetEdit("HwRange", ms.RangeM.ToString());
            SetEdit("HwPower", ms.PowerFactor.ToString());
            SetEdit("HwSpy",   ms.SpyMinutes.ToString());
            SetEdit("HwPages", JoinPages(ms.EnablesPages));
            SetEdit("HwMarks", "");
            m_HwWritable = true;
        }
        else
        {
            OZ_CarrierSpec cs = m_HwCfg.Carriers[idx];
            SetEdit("HwClass", cs.ClassName);
            SetEdit("HwName",  cs.DisplayName);
            SetEdit("HwKind",  cs.DefaultKind);
            SetEdit("HwRange", "");
            SetEdit("HwPower", "");
            SetEdit("HwSpy",   "");
            SetEdit("HwPages", "");
            // Одне число замість двох: місткість носія тепер рахується в
            // ЗАПИСАХ, байдуже яких. Див. OZ_CarrierSpec.
            SetEdit("HwMarks", cs.MaxRecords.ToString());
            m_HwWritable = cs.Writable;
        }
        PaintHwToggles();
        Hint(kind + ": " + GetEdit("HwClass"));
    }

    private void NewHwForm(string kind)
    {
        m_HwPickedKind = kind;
        m_HwPickedIdx  = -1;
        m_HwNewMode    = true;
        m_HwDelArmed   = false;

        SetEdit("HwClass", "");
        SetEdit("HwName",  "");
        SetEdit("HwRange", "0");
        SetEdit("HwPower", "1.0");
        SetEdit("HwSpy",   "0");
        SetEdit("HwPages", "");
        SetEdit("HwMarks", "0");
        if (kind == "module")
            SetEdit("HwKind", "antenna");
        else
            SetEdit("HwKind", "markers");
        m_HwWritable = true;
        PaintHwToggles();
        Hint("new " + kind + ": classname is the key, then SAVE");
    }

    private void PaintHwToggles()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnHwWritableText"));
        if (!t)
            return;
        if (m_HwWritable)
            t.SetText("writable: yes");
        else
            t.SetText("writable: no");
    }

    private void SaveHwForm()
    {
        if (!m_HwCfg)
        {
            Hint("Hardware is not loaded yet");
            return;
        }

        string cls = GetEdit("HwClass");
        if (cls == "" || cls.IndexOf(" ") != -1)
        {
            Hint("classname must be a single word");
            return;
        }

        if (m_HwPickedKind == "module")
        {
            OZ_ModuleSpec ms;
            if (m_HwNewMode)
            {
                ms = new OZ_ModuleSpec();
                if (!m_HwCfg.Modules)
                    m_HwCfg.Modules = new array<ref OZ_ModuleSpec>();
                m_HwCfg.Modules.Insert(ms);
                m_HwPickedIdx = m_HwCfg.Modules.Count() - 1;
                m_HwNewMode = false;
            }
            else
            {
                if (m_HwPickedIdx < 0 || m_HwPickedIdx >= m_HwCfg.Modules.Count())
                {
                    Hint("pick an entry first");
                    return;
                }
                ms = m_HwCfg.Modules[m_HwPickedIdx];
            }

            ms.ClassName   = cls;
            ms.DisplayName = GetEdit("HwName");
            ms.Kind        = GetEdit("HwKind");
            ms.RangeM      = GetEdit("HwRange").ToFloat();
            ms.PowerFactor = GetEdit("HwPower").ToFloat();
            ms.SpyMinutes  = GetEdit("HwSpy").ToFloat();
            ms.EnablesPages = SplitPages(GetEdit("HwPages"));
        }
        else if (m_HwPickedKind == "carrier")
        {
            OZ_CarrierSpec cs;
            if (m_HwNewMode)
            {
                cs = new OZ_CarrierSpec();
                if (!m_HwCfg.Carriers)
                    m_HwCfg.Carriers = new array<ref OZ_CarrierSpec>();
                m_HwCfg.Carriers.Insert(cs);
                m_HwPickedIdx = m_HwCfg.Carriers.Count() - 1;
                m_HwNewMode = false;
            }
            else
            {
                if (m_HwPickedIdx < 0 || m_HwPickedIdx >= m_HwCfg.Carriers.Count())
                {
                    Hint("pick an entry first");
                    return;
                }
                cs = m_HwCfg.Carriers[m_HwPickedIdx];
            }

            cs.ClassName   = cls;
            cs.DisplayName = GetEdit("HwName");
            cs.DefaultKind = GetEdit("HwKind");
            cs.Writable    = m_HwWritable;
            cs.MaxRecords  = GetEdit("HwMarks").ToInt();
        }
        else
        {
            Hint("pick an entry or press NEW first");
            return;
        }

        PushHwCfg();
    }

    private void DeleteHwEntry()
    {
        if (!m_HwCfg || m_HwPickedIdx < 0 || m_HwNewMode)
        {
            Hint("pick an entry first");
            return;
        }

        if (!m_HwDelArmed)
        {
            m_HwDelArmed = true;
            Hint("press DELETE again to remove " + GetEdit("HwClass"));
            return;
        }
        m_HwDelArmed = false;

        if (m_HwPickedKind == "module")
            m_HwCfg.Modules.Remove(m_HwPickedIdx);
        else
            m_HwCfg.Carriers.Remove(m_HwPickedIdx);

        m_HwPickedIdx = -1;
        m_HwPickedKind = "";
        PushHwCfg();
    }

    private void PushHwCfg()
    {
        string body;
        string err;
        if (!JsonFileLoader<OZ_PdaHardwareConfig>.MakeData(m_HwCfg, body, err, false))
        {
            Hint("cannot serialise");
            return;
        }
        SendCfg("Hardware", body);
    }

    private string JoinPages(array<string> pages)
    {
        if (!pages)
            return "";
        string outp = "";
        for (int i = 0; i < pages.Count(); i++)
        {
            if (outp != "")
                outp += " ";
            outp += pages[i];
        }
        return outp;
    }

    private ref array<string> SplitPages(string text)
    {
        array<string> outp = new array<string>();
        array<string> parts = new array<string>();
        text.Split(" ", parts);
        for (int i = 0; i < parts.Count(); i++)
        {
            if (parts[i] != "")
                outp.Insert(parts[i]);
        }
        return outp;
    }

    // ---------------------------------------------------------- ввiд

    override bool OnItemSelected(Widget w, int x, int y, int row, int column, int oldRow, int oldColumn)
    {
        if (w && w.GetName() == "HwList")
        {
            if (row >= 0 && row < m_HwRowKind.Count())
                FillHwForm(m_HwRowKind[row], m_HwRowIdx[row]);
            return true;
        }
        return super.OnItemSelected(w, x, y, row, column, oldRow, oldColumn);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w && M_SUB_WIDGET)
        {
            string nm = w.GetName();

            if (nm == "BtnTunSave")
            {
                SaveTuningForm();
                return true;
            }

            if (nm == "BtnPdaSubHw" || nm == "BtnPdaSubProf")
            {
                Widget hw = M_SUB_WIDGET.FindAnyWidget("PdaHwBox");
                Widget pf = M_SUB_WIDGET.FindAnyWidget("PdaProfBox");
                bool wantHw = (nm == "BtnPdaSubHw");
                if (hw)
                    hw.Show(wantHw);
                if (pf)
                    pf.Show(!wantHw);
                if (!wantHw)
                    AskCfg("Profiles");
                return true;
            }

            if (nm == "BtnHwSave")
            {
                SaveHwForm();
                return true;
            }

            if (nm == "BtnHwDel")
            {
                DeleteHwEntry();
                return true;
            }

            if (nm == "BtnHwNewMod")
            {
                NewHwForm("module");
                return true;
            }

            if (nm == "BtnHwNewCar")
            {
                NewHwForm("carrier");
                return true;
            }

            if (nm == "BtnHwWritable")
            {
                m_HwWritable = !m_HwWritable;
                PaintHwToggles();
                return true;
            }

            if (nm == "BtnProfReload")
            {
                AskCfg("Profiles");
                return true;
            }

            if (nm == "BtnProfApply")
            {
                MultilineEditBoxWidget ed = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("PdaProfEdit"));
                if (ed)
                {
                    string body;
                    ed.GetText(body);
                    SendCfg("Profiles", body);
                }
                return true;
            }
        }

        return super.OnClick(w, x, y, button);
    }
}

#endif
#endif
#endif
