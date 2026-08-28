// Редактор розкладки HUD: тягни рамки, APPLY зберігає, RESET повертає
// типові місця, CANCEL лишає як було.
//
// Живий HUD на час редагування схований сам собою: Visible() гасить його,
// поки будь-яке меню відкрите. Замість нього гравець тягає ПРОКСИ -- рамки
// з підписами тих самих розмірів на тих самих місцях.

class OZ_HudEditMenu : UIScriptedMenu
{
    // Прокси й панель, яку кожен представляє. Індекси паралельні.
    private ref array<Widget> m_Proxies;
    private ref array<ref OZ_HudPane> m_Panes;

    private Widget m_Drag;
    private float m_GrabDX;
    private float m_GrabDY;

    private ButtonWidget m_BtnApply;
    private ButtonWidget m_BtnReset;
    private ButtonWidget m_BtnCancel;

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_hud_edit.layout");

        m_BtnApply  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnHudApply"));
        m_BtnReset  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnHudReset"));
        m_BtnCancel = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnHudCancel"));

        TextWidget hint = TextWidget.Cast(layoutRoot.FindAnyWidget("EditHint"));
        if (hint)
            hint.SetText("#STR_OZ_HUD_EDIT_HINT");
        SetText("BtnHudApplyText", "#STR_OZ_HUD_APPLY");
        SetText("BtnHudResetText", "#STR_OZ_HUD_RESET");
        SetText("BtnHudCancelText", "#STR_OZ_HUD_CANCEL");

        m_Proxies = new array<Widget>();
        m_Panes   = new array<ref OZ_HudPane>();

        array<ref OZ_HudPane> panes = OZ_PdaHud.Panes();
        for (int i = 0; i < panes.Count(); i++)
        {
            OZ_HudPane pane = panes[i];
            if (!pane.W)
                continue;

            Widget proxy = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_hud_proxy.layout", layoutRoot);
            if (!proxy)
                continue;

            float x;
            float y;
            float w;
            float h;
            pane.W.GetPos(x, y);
            pane.W.GetSize(w, h);

            proxy.SetPos(x, y);
            proxy.SetSize(w, h);

            TextWidget label = TextWidget.Cast(proxy.FindAnyWidget("ProxyLabel"));
            if (label)
                label.SetText(pane.Label);

            m_Proxies.Insert(proxy);
            m_Panes.Insert(pane);
        }

        return layoutRoot;
    }

    private void SetText(string name, string value)
    {
        TextWidget t = TextWidget.Cast(layoutRoot.FindAnyWidget(name));
        if (t)
            t.SetText(value);
    }

    override void OnShow()
    {
        super.OnShow();
        GetGame().GetUIManager().ShowUICursor(true);
        GetGame().GetMission().PlayerControlDisable(INPUT_EXCLUDE_ALL);
    }

    override void OnHide()
    {
        GetGame().GetMission().PlayerControlEnable(true);
        super.OnHide();
    }

    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (button != MouseState.LEFT)
            return super.OnMouseButtonDown(w, x, y, button);

        // Прокси лежать прямо в корені -- шукаємо, чи натиснули одну з них.
        for (int i = 0; i < m_Proxies.Count(); i++)
        {
            if (w == m_Proxies[i])
            {
                m_Drag = w;

                float px;
                float py;
                w.GetPos(px, py);

                float mx;
                float my;
                MouseFrac(mx, my);

                m_GrabDX = mx - px;
                m_GrabDY = my - py;
                return true;
            }
        }

        return super.OnMouseButtonDown(w, x, y, button);
    }

    override bool OnMouseButtonUp(Widget w, int x, int y, int button)
    {
        if (m_Drag)
        {
            m_Drag = null;
            return true;
        }
        return super.OnMouseButtonUp(w, x, y, button);
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);

        if (!m_Drag)
            return;

        float mx;
        float my;
        MouseFrac(mx, my);

        float w;
        float h;
        m_Drag.GetSize(w, h);

        float nx = Math.Clamp(mx - m_GrabDX, 0, 1 - w);
        float ny = Math.Clamp(my - m_GrabDY, 0, 1 - h);
        m_Drag.SetPos(nx, ny);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_BtnApply)
        {
            for (int i = 0; i < m_Proxies.Count(); i++)
            {
                float px;
                float py;
                m_Proxies[i].GetPos(px, py);
                OZ_PdaHudLayout.Put(m_Panes[i].Id, px, py);
            }

            OZ_PdaHudLayout.Save();
            OZ_PdaHud.Reapply();
            Close();
            return true;
        }

        if (w == m_BtnReset)
        {
            // Повернути ТИПОВІ місця -- і на прокси, і в збереження: RESET
            // означає «як з коробки», а не «як було хвилину тому».
            for (int k = 0; k < m_Proxies.Count(); k++)
            {
                m_Proxies[k].SetPos(m_Panes[k].DefX, m_Panes[k].DefY);
                OZ_PdaHudLayout.Forget(m_Panes[k].Id);
            }

            OZ_PdaHudLayout.Save();
            OZ_PdaHud.Reapply();
            return true;
        }

        if (w == m_BtnCancel)
        {
            Close();
            return true;
        }

        return super.OnClick(w, x, y, button);
    }

    override bool OnKeyPress(Widget w, int x, int y, int key)
    {
        if (key == KeyCode.KC_ESCAPE)
        {
            Close();
            return true;
        }
        return super.OnKeyPress(w, x, y, key);
    }

    private void MouseFrac(out float fx, out float fy)
    {
        int mx;
        int my;
        GetMousePos(mx, my);

        float sw;
        float sh;
        GetGame().GetWorkspace().GetScreenSize(sw, sh);

        fx = 0;
        fy = 0;
        if (sw > 0)
            fx = mx / sw;
        if (sh > 0)
            fy = my / sh;
    }
}
