// Сторінка «Фракція»: свої люди і фракційні дії. Все, що робить лідер,
// їде через OZ_Rpc.RoleRequest -- тим самим каналом, яким це робили
// контакти, поки фракційні кнопки жили там.
//
// ДІЛИТЬ ВКЛАДКУ З КОНТАКТАМИ (рішення власника 2026-08-30): ліворуч люди,
// праворуч свої. Сторінки лишились дві -- два обробники, два конверти, два
// незалежні оновлення; спільна в них тільки кнопка вкладки, і зшиває їх
// меню (див. m_Companion в OZ_PdaMenu), а не цей файл.
//
// БАЗОВА ФРАКЦІЯ сюди не доходить: сервер віддає «сталкерам» порожню
// фракцію, і половина екрана чесно каже NO FACTION. Сталкери -- це всі в
// Зоні, а не організація: складу в неї немає, лідера немає, і поіменний
// перелік усіх сталкерів сервера тут був би і безглуздий, і зайве
// розголошення.

class OZ_PdaPageFaction : OZ_PdaPage
{
    private int m_Beat = 0;
    private ref OZ_FactionState m_St;
    private Widget m_Rows;
    private ref array<Widget> m_RowWgts;
    private int m_RowsY = 0;
    private string m_Picked = "";     // ім'я обраного члена
    private string m_PickedCand = ""; // ім'я обраного кандидата

    private ButtonWidget m_BtnKick;
    private ButtonWidget m_BtnLead;
    private ButtonWidget m_BtnInvite;
    private ButtonWidget m_BtnJoin;
    private ButtonWidget m_BtnRefuse;

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_faction.layout";
    }

    override void OnBuilt()
    {
        m_Rows    = Wgt("FactionRows");
        m_RowWgts = new array<Widget>();

        m_BtnKick   = ButtonWidget.Cast(Wgt("BtnFKick"));
        SetText("BtnFKickText", "#STR_OZ_FACTION_KICK");
        m_BtnLead   = ButtonWidget.Cast(Wgt("BtnFLead"));
        SetText("BtnFLeadText", "#STR_OZ_FACTION_LEAD");
        m_BtnInvite = ButtonWidget.Cast(Wgt("BtnFInvite"));
        SetText("BtnFInviteText", "#STR_OZ_FACTION_INVITE");
        m_BtnJoin   = ButtonWidget.Cast(Wgt("BtnFJoin"));
        SetText("BtnFJoinText", "#STR_OZ_FACTION_JOIN");
        m_BtnRefuse = ButtonWidget.Cast(Wgt("BtnFRefuse"));
        SetText("BtnFRefuseText", "#STR_OZ_FACTION_REFUSE");
    }

    override void OnSelected()
    {
        Request();
    }

    override void OnRefresh()
    {
        // Ролі їдуть через Discord і повертаються не миттєво: рідкий
        // самооновлювач страхує пуш, а не замінює його. Раз на 5 секунд
        // -- зміни й так приходять пушем одразу.
        m_Beat++;
        if (m_Beat % 5 != 0)
            return;
        Request();
    }

    private void Request()
    {
        OZ_Rpc.Request(OZ_PdaConst.PAGE_FACTION, "state", "{}");
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        if (op == "push")
        {
            Request();
            return;
        }

        if (op != "state")
            return;

        if (!ok)
        {
            SetHintSticky("FactionHint", "#" + error);
            return;
        }

        string err;
        OZ_FactionState st;
        if (!JsonFileLoader<OZ_FactionState>.LoadData(json, st, err) || !st)
            return;

        m_St = st;
        Paint();
    }

    private void Paint()
    {
        if (!m_St)
            return;

        // Шапка: чия це сторінка. Одинак бачить чесне «фракції немає».
        if (m_St.Faction == "")
        {
            SetText("FactionTitle", "#STR_OZ_FACTION_NONE");
            SetText("FactionMine", "");
        }
        else
        {
            TextWidget t = TextWidget.Cast(Wgt("FactionTitle"));
            if (t)
            {
                t.SetText(m_St.FactionName);
                if (m_St.Color != 0)
                    t.SetColor(m_St.Color);
            }

            string mine = m_St.MyRank;
            if (m_St.MeLeader)
            {
                if (mine != "")
                    mine += "  ";
                mine += Widget.TranslateString("#STR_OZ_FACTION_LEADER");
            }
            SetText("FactionMine", mine);
        }

        // Запрошення: банер з двома кнопками.
        bool invited = m_St.InviteFaction != "";
        Widget banner = Wgt("InvitePane");
        if (banner)
            banner.Show(invited);
        if (invited)
            SetText("InviteText", m_St.InviteFaction + "  --  " + m_St.InviteFrom);

        // Члени.
        for (int r = 0; r < m_RowWgts.Count(); r++)
        {
            if (m_RowWgts[r])
                m_RowWgts[r].Unlink();
        }
        m_RowWgts.Clear();
        m_RowsY = 0;

        if (m_Rows && m_St.Members)
        {
            for (int i = 0; i < m_St.Members.Count(); i++)
                MemberRow(m_St.Members[i]);

            // Висота канви чесна: коли всі влазять, повзунок не потрібен.
            // Ширина -- рядка фракції в правій половині спільної вкладки.
            m_Rows.SetSize(616, m_RowsY);
        }

        // Кандидати -- текстом під діями (вибір циклічний кнопкою).
        PaintCandidate();
        PaintButtons();
    }

    private void MemberRow(OZ_FactionMember m)
    {
        Widget w = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_faction_row.layout", m_Rows);
        if (!w)
            return;

        w.SetPos(0, m_RowsY);
        m_RowsY += 34;
        w.SetName(m.Name);
        w.SetUserID(8);
        m_RowWgts.Insert(w);

        TextWidget n = TextWidget.Cast(w.FindAnyWidget("FRowName"));
        if (n)
        {
            string label = m.Name;
            if (m.Leader)
                label = "* " + label;
            n.SetText(label);
            if (m.Me)
                n.SetColor(ARGB(255, 255, 122, 26));
            else if (m.Online)
                n.SetColor(ARGB(255, 126, 200, 160));
        }

        TextWidget rk = TextWidget.Cast(w.FindAnyWidget("FRowRank"));
        if (rk)
            rk.SetText(m.Rank);

        // Зебра: парні рядки трохи світліші, оку легше вести рядок.
        Widget bg = w.FindAnyWidget("FRowBg");
        if (bg && (m_RowWgts.Count() % 2) == 0)
            bg.SetColor(ARGB(255, 20, 20, 23));

        Widget pick = w.FindAnyWidget("FRowPick");
        if (pick)
            pick.Show(m.Name == m_Picked);
    }

    private void PaintCandidate()
    {
        string label = "";
        if (m_St && m_St.MeLeader && m_St.Candidates && m_St.Candidates.Count() > 0)
        {
            if (m_PickedCand == "" || m_St.Candidates.Find(m_PickedCand) == -1)
                m_PickedCand = m_St.Candidates[0];
            label = Widget.TranslateString("#STR_OZ_FACTION_CAND") + "  " + m_PickedCand + "  (" + m_St.Candidates.Count().ToString() + ")";
        }
        else
            m_PickedCand = "";

        SetText("CandText", label);
    }

    private void PaintButtons()
    {
        bool lead = m_St && m_St.MeLeader;
        bool pickedOther = m_Picked != "" && m_St && !PickedIsMe();

        if (m_BtnKick)
            m_BtnKick.Show(lead && pickedOther);
        if (m_BtnLead)
            m_BtnLead.Show(lead && pickedOther);
        if (m_BtnInvite)
            m_BtnInvite.Show(lead && m_PickedCand != "");
    }

    private bool PickedIsMe()
    {
        if (!m_St || !m_St.Members)
            return false;
        for (int i = 0; i < m_St.Members.Count(); i++)
        {
            if (m_St.Members[i].Name == m_Picked)
                return m_St.Members[i].Me;
        }
        return false;
    }

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

        if (w.GetUserID() == 8)
        {
            m_Picked = w.GetName();
            Paint();
            return true;
        }

        if (w == m_BtnJoin)
        {
            OZ_Rpc.RoleRequest("accept", "", "");
            return true;
        }

        if (w == m_BtnRefuse)
        {
            OZ_Rpc.RoleRequest("decline", "", "");
            return true;
        }

        if (w == m_BtnKick)
        {
            if (m_Picked != "")
                OZ_Rpc.RoleRequest(OZ_RoleOp.FACTION_CLEAR, m_Picked, "");
            return true;
        }

        if (w == m_BtnLead)
        {
            if (m_Picked != "")
                OZ_Rpc.RoleRequest(OZ_RoleOp.LEADER_TRANSFER, m_Picked, "");
            return true;
        }

        if (w == m_BtnInvite)
        {
            // Клік по тексту кандидата циклить список; кнопка кличе обраного.
            if (m_PickedCand != "")
                OZ_Rpc.RoleRequest("invite", m_PickedCand, "");
            return true;
        }

        // Клік по рядку кандидатів циклить вибір.
        if (w.GetName() == "CandText" || w.GetName() == "BtnCandNext")
        {
            CycleCandidate();
            return true;
        }

        return false;
    }

    private void CycleCandidate()
    {
        if (!m_St || !m_St.Candidates || m_St.Candidates.Count() == 0)
            return;

        int at = m_St.Candidates.Find(m_PickedCand);
        at = (at + 1) % m_St.Candidates.Count();
        m_PickedCand = m_St.Candidates[at];
        PaintCandidate();
        PaintButtons();
    }
}
