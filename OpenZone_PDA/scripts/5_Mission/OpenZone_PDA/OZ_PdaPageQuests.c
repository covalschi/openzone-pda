// Сторінка «Журнал»: завдання, які дав квестовий мод.
//
// КПК не знає жодного квесту. Він показує те, що прийшло по договору
// OZ_PdaQuests, і розрізняє три різні стани, які легко переплутати:
//
//   немає постачальника -- на сервері квестового мода взагалі немає;
//   постачальник є, завдань нуль -- усе виконано або ще не взято;
//   завдання є -- малюємо список.
//
// Перші два виглядають однаково («нічого не видно»), а означають протилежне,
// і гравець мусить бачити різницю.

class OZ_PdaPageQuests : OZ_PdaPage
{
    private TextListboxWidget m_List;
    private ref OZ_QuestJournal m_Journal;

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_quests.layout";
    }

    override void OnBuilt()
    {
        m_List = TextListboxWidget.Cast(Wgt("QuestList"));
    }

    override void OnSelected()
    {
        OZ_Rpc.Request(OZ_PdaConst.PAGE_QUESTS, "journal", "{}");
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        if (op != "journal")
            return;

        if (!ok)
        {
            SetText("QuestHint", "#" + error);
            return;
        }

        string err;
        OZ_QuestJournal j;
        if (!JsonFileLoader<OZ_QuestJournal>.LoadData(json, j, err))
        {
            OZ_Log.Error("quest journal unreadable: " + err);
            return;
        }

        m_Journal = j;
        Paint();
    }

    private void Paint()
    {
        if (m_List)
            m_List.ClearItems();

        if (!m_Journal.HasProvider)
        {
            SetText("QuestSource", "");
            SetText("QuestHint", "#STR_OZ_QUESTS_NO_PROVIDER");
            return;
        }

        SetText("QuestSource", m_Journal.ProviderName);

        if (m_Journal.Entries.Count() == 0)
        {
            SetText("QuestHint", "#STR_OZ_QUESTS_EMPTY");
            return;
        }

        SetText("QuestHint", "");

        for (int i = 0; i < m_Journal.Entries.Count(); i++)
        {
            OZ_QuestEntry e = m_Journal.Entries[i];

            string row = StateMark(e.State);
            row += "  " + e.Title;

            int done = 0;
            for (int o = 0; e.Objectives && o < e.Objectives.Count(); o++)
            {
                if (e.Objectives[o].Done)
                    done++;
            }

            if (e.Objectives && e.Objectives.Count() > 0)
            {
                row += "   " + done.ToString();
                row += "/" + e.Objectives.Count().ToString();
            }

            if (m_List)
                m_List.AddItem(row, NULL, 0);
        }
    }

    // Стан рядком, а не кольором: колір у списку задається стилем, і
    // сперечатися з ним заради трьох станів не варто.
    private string StateMark(string state)
    {
        if (state == "done")
            return "[+]";
        if (state == "failed")
            return "[x]";
        return "[ ]";
    }
}
