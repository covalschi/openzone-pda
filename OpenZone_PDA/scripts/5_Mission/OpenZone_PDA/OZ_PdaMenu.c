// Меню КПК: корпус пристрою поверх приглушеного світу.
//
// Стрічка вкладок будується не з реєстру й не з конфіга, а з ВІДПОВІДІ
// СЕРВЕРА про той пристрій, що в руках. Причина: профілі серверні, і
// вирішувати, які вкладки в тебе є, не має права клієнт. Тому меню
// відкривається порожнім, надсилає device/status і домальовує себе.
//
// Один запит, а не два: сторінка «Пристрій» усе одно питає той самий status,
// і її ж відповідь будує стрічку.

class OZ_PdaMenu : UIScriptedMenu
{
    private Widget m_TabRail;
    private Widget m_PageHost;
    private Widget m_LockPanel;
    private ButtonWidget m_BtnClose;

    private ref map<string, ref OZ_PdaPage> m_Pages;
    private ref array<Widget> m_Tabs;
    private string m_Current = "";
    private bool m_Built = false;

    private ref Timer m_Refresh;

    // Введений код накопичується тут і НІКУДИ більше: на сервер їде рівно
    // один раз, коли гравець підтвердив. Порівнює його сервер.
    private string m_PinBuffer = "";

    void OZ_PdaMenu()
    {
        m_Pages = new map<string, ref OZ_PdaPage>();
        m_Tabs  = new array<Widget>();
    }

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_menu.layout");
        if (!layoutRoot)
            return null;

        m_TabRail   = layoutRoot.FindAnyWidget("TabRail");
        m_PageHost  = layoutRoot.FindAnyWidget("PageHost");
        m_LockPanel = layoutRoot.FindAnyWidget("LockPanel");
        m_BtnClose  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnClose"));

        return layoutRoot;
    }

    // Пастка, на якій горіли інші моди: якщо layout не завантажився, синглтон
    // меню лишається «відкритим» і блокує ВСІ меню гри назавжди. Тому перша ж
    // дія при показі -- перевірити, що дерево взагалі є.
    override void OnShow()
    {
        super.OnShow();

        if (!GetLayoutRoot())
        {
            OZ_Log.Error("pda layout failed to load - closing to avoid a ghost menu");
            GetGame().GetUIManager().CloseMenu(OZ_PdaConst.MENU_PDA);
            return;
        }

        SetFocus(layoutRoot);

        array<string> excludes = new array<string>();
        excludes.Insert("menu");
        GetGame().GetMission().AddActiveInputExcludes(excludes);
        GetGame().GetMission().GetHud().Show(false);

        OZ_ClientState.BindListener(new OZ_PdaMenuListener(this));

        // Питаємо сервер, що в нас за пристрій. До відповіді стрічка порожня.
        OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "status", "{}");

        if (!m_Refresh)
            m_Refresh = new Timer(CALL_CATEGORY_GUI);
        m_Refresh.Run(1.0, this, "RefreshTick", NULL, true);
    }

    override void OnHide()
    {
        super.OnHide();

        if (m_Refresh)
            m_Refresh.Stop();

        OZ_ClientState.BindListener(null);

        array<string> excludes = new array<string>();
        excludes.Insert("menu");
        GetGame().GetMission().RemoveActiveInputExcludes(excludes, true);
        GetGame().GetMission().GetHud().Show(true);
    }

    void RefreshTick()
    {
        if (m_Current != "" && m_Pages.Contains(m_Current))
            m_Pages.Get(m_Current).OnRefresh();
    }

    // ----------------------------------------------------------- відповіді

    void HandleResponse(string pageId, string op, bool ok, string json, string error)
    {
        // Стрічку будує ПЕРША ж відповідь про пристрій -- і тільки один раз.
        if (!m_Built && pageId == OZ_PdaConst.PAGE_DEVICE && op == "status" && ok)
            BuildFrom(json);

        if (pageId == OZ_PdaConst.PAGE_DEVICE && op == "status")
        {
            ApplyLockState(ok, json, error);
            PaintStatusBar(ok, json);
        }

        if (m_Pages.Contains(pageId))
            m_Pages.Get(pageId).OnResponse(op, ok, json, error);

        if (!ok && pageId == OZ_PdaConst.PAGE_DEVICE && op == "unlock")
            OnBadPin();
    }

    private void BuildFrom(string json)
    {
        string err;
        OZ_PdaDeviceStatus st;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.LoadData(json, st, err))
        {
            OZ_Log.Error("device status unreadable while building tabs: " + err);
            return;
        }

        m_Built = true;

        // Порядок задає ПРОФІЛЬ, не реєстр: адмін вирішує, що йде першим.
        for (int i = 0; i < st.Pages.Count(); i++)
            AddTab(st.Pages[i]);

        if (st.Pages.Count() > 0)
            Select(st.Pages[0]);
    }

    private void AddTab(string pageId)
    {
        OZ_PdaPage page = OZ_PdaPageFactory.Make(pageId);
        if (!page)
            return;   // клієнт не вміє малювати -- вкладки не буде, причина в лозі

        Widget tab = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_tab.layout", m_TabRail);
        if (!tab)
            return;

        tab.SetName(pageId);
        tab.SetUserID(1);         // так OnClick відрізняє вкладку від решти

        TextWidget glyph = TextWidget.Cast(tab.FindAnyWidget("TabGlyph"));
        if (glyph)
            glyph.SetText(OZ_PdaPageFactory.Glyph(pageId));

        m_Tabs.Insert(tab);

        page.Init(pageId, m_PageHost);
        page.Show(false);
        m_Pages.Insert(pageId, page);
    }

    void Select(string pageId)
    {
        if (m_Current == pageId)
            return;

        if (m_Current != "" && m_Pages.Contains(m_Current))
        {
            m_Pages.Get(m_Current).Show(false);
            m_Pages.Get(m_Current).OnDeselected();
        }

        m_Current = pageId;

        if (m_Pages.Contains(pageId))
        {
            m_Pages.Get(pageId).Show(true);
            m_Pages.Get(pageId).OnSelected();
        }

        PaintTabs();
    }

    private void PaintTabs()
    {
        for (int i = 0; i < m_Tabs.Count(); i++)
        {
            Widget t = m_Tabs[i];
            bool active = (t.GetName() == m_Current);

            Widget mark = t.FindAnyWidget("TabActive");
            if (mark)
                mark.Show(active);

            TextWidget glyph = TextWidget.Cast(t.FindAnyWidget("TabGlyph"));
            if (glyph)
            {
                if (active)
                    glyph.SetColor(ARGB(255, 13, 13, 15));
                else
                    glyph.SetColor(ARGB(255, 93, 93, 99));
            }
        }
    }

    // ---------------------------------------------------------------- замок

    private void ApplyLockState(bool ok, string json, string error)
    {
        if (!m_LockPanel)
            return;

        // Відмова саме через замок -- єдина причина показати екран коду.
        if (!ok)
        {
            bool locked = (error == "STR_OZ_ERR_NO_ACCESS");
            m_LockPanel.Show(locked);
            return;
        }

        string err;
        OZ_PdaDeviceStatus st;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.LoadData(json, st, err))
            return;

        bool needPin = st.HasPin && !st.Unlocked;
        m_LockPanel.Show(needPin);

        if (needPin)
        {
            TextWidget hint = TextWidget.Cast(layoutRoot.FindAnyWidget("LockHint"));
            if (hint)
            {
                if (st.LockedOut)
                    hint.SetText("#STR_OZ_LOCK_TOO_MANY");
                else
                    hint.SetText("");
            }
        }
    }

    // Смуга стану -- те, що гравець мусить бачити, не заходячи на сторінку:
    // живлення, стан зв'язку й час. Ті самі дані, що вже прийшли; окремого
    // запиту не робимо.
    private void PaintStatusBar(bool ok, string json)
    {
        TextWidget left  = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusLeft"));
        TextWidget mid   = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusMid"));
        TextWidget right = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusRight"));

        if (!ok)
        {
            if (left)  left.SetText("#STR_OZ_DEV_OFF");
            if (mid)   mid.SetText("");
            if (right) right.SetText("");
            return;
        }

        string err;
        OZ_PdaDeviceStatus st;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.LoadData(json, st, err))
            return;

        if (left)
        {
            if (!st.Powered)
                left.SetText("#STR_OZ_DEV_OFF");
            else
            {
                int pct = Math.Round(st.Charge01 * 100);
                string l = "#STR_OZ_DEV_POWER";
                l += "  " + pct.ToString() + "%";
                left.SetText(l);
            }
        }

        if (mid)
        {
            if (st.Online)
                mid.SetText("");
            else
                mid.SetText("#STR_OZ_DEV_OFFLINE_SHORT");
        }

        if (right)
        {
            // Час беремо ігровий: гравцеві потрібен час Зони, а не свій
            // системний.
            // GetHours/GetMinutes не існує -- рушій віддає дату цілком одним
            // викликом: GetDate(out year, month, day, hour, minute).
            int y, mo, d, h, m;
            GetGame().GetWorld().GetDate(y, mo, d, h, m);
            string tm = Pad2(h);
            tm += ":" + Pad2(m);
            right.SetText(tm);
        }
    }

    private string Pad2(int v)
    {
        if (v < 10)
            return "0" + v.ToString();
        return v.ToString();
    }

    private void PaintPinDots()
    {
        TextWidget dots = TextWidget.Cast(layoutRoot.FindAnyWidget("LockDots"));
        if (!dots)
            return;

        string s = "";
        for (int i = 0; i < 4; i++)
        {
            if (i < m_PinBuffer.Length())
                s += "*";
            else
                s += "-";
        }
        dots.SetText(s);
    }

    private void OnBadPin()
    {
        m_PinBuffer = "";
        PaintPinDots();

        TextWidget hint = TextWidget.Cast(layoutRoot.FindAnyWidget("LockHint"));
        if (hint)
            hint.SetText("#STR_OZ_LOCK_WRONG");
    }

    override bool OnKeyPress(Widget w, int x, int y, int key)
    {
        if (key == KeyCode.KC_ESCAPE)
        {
            Close();
            return true;
        }

        // Код набирається лише поки видно екран блокування.
        if (m_LockPanel && m_LockPanel.IsVisible())
        {
            if (key >= KeyCode.KC_0 && key <= KeyCode.KC_9 && m_PinBuffer.Length() < 4)
            {
                m_PinBuffer += (key - KeyCode.KC_0).ToString();
                PaintPinDots();
                return true;
            }

            if (key == KeyCode.KC_BACK && m_PinBuffer.Length() > 0)
            {
                m_PinBuffer = m_PinBuffer.Substring(0, m_PinBuffer.Length() - 1);
                PaintPinDots();
                return true;
            }

            if (key == KeyCode.KC_RETURN && m_PinBuffer.Length() > 0)
            {
                OZ_PdaPinAttempt att = new OZ_PdaPinAttempt();
                att.Pin = m_PinBuffer;

                string json;
                string err;
                if (JsonFileLoader<OZ_PdaPinAttempt>.MakeData(att, json, err, false))
                    OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "unlock", json);

                return true;
            }
        }

        return super.OnKeyPress(w, x, y, key);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_BtnClose)
        {
            Close();
            return true;
        }

        if (w && w.GetUserID() == 1)
        {
            Select(w.GetName());
            return true;
        }

        // Далі -- активна сторінка. Питаємо тільки її: сторінки, яку не видно,
        // клікнути неможливо, і давати їй голос означало б ловити чужі кнопки.
        if (m_Current != "" && m_Pages.Contains(m_Current))
        {
            if (m_Pages.Get(m_Current).OnPageClick(w))
                return true;
        }

        return super.OnClick(w, x, y, button);
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (w && w.GetUserID() == 1)
        {
            Widget hover = w.FindAnyWidget("TabHover");
            if (hover)
                hover.Show(true);
            return true;
        }
        return super.OnMouseEnter(w, x, y);
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (w && w.GetUserID() == 1)
        {
            Widget hover = w.FindAnyWidget("TabHover");
            if (hover)
                hover.Show(false);
            return true;
        }
        return super.OnMouseLeave(w, enterW, x, y);
    }
}

// Тонкий перехідник: ядро кличе слухача, слухач кличе меню. Меню не може
// успадкувати OZ_ResponseListener саме, бо воно вже UIScriptedMenu.
class OZ_PdaMenuListener : OZ_ResponseListener
{
    private OZ_PdaMenu m_Menu;

    void OZ_PdaMenuListener(OZ_PdaMenu menu)
    {
        m_Menu = menu;
    }

    override void OnResponse(string pageId, string op, bool ok, string json, string error)
    {
        if (m_Menu)
            m_Menu.HandleResponse(pageId, op, ok, json, error);
    }
}
