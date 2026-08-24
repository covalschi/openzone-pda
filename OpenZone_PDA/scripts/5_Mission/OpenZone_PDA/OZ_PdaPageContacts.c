// Сторінка «Контакти»: хто зараз у Зоні.
//
// Список приходить із сервера вже відфільтрованим: невидимок у ньому немає
// зовсім, і клієнту нема чого приховувати -- бо нема чого й отримувати.
// Лічильник рахує рівно те, що видно, і другого числа тут не буває: інакше
// різниця між ними й була б відповіддю на питання «скільки невидимок».

class OZ_PdaPageContacts : OZ_PdaPage
{
    private TextListboxWidget m_List;
    private ButtonWidget m_BtnHide;
    private ref OZ_ContactList m_Data;

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_contacts.layout";
    }

    override void OnBuilt()
    {
        m_List    = TextListboxWidget.Cast(Wgt("ContactList"));
        m_BtnHide = ButtonWidget.Cast(Wgt("BtnHide"));
    }

    override void OnSelected()
    {
        Request();
    }

    // Раз на секунду: люди заходять і виходять самі, і список мусить це
    // показувати без натискань.
    override void OnRefresh()
    {
        Request();
    }

    private void Request()
    {
        OZ_Rpc.Request(OZ_PdaConst.PAGE_CONTACTS, "list", "{}");
    }

    override bool OnPageClick(Widget w)
    {
        if (w && w == m_BtnHide)
        {
            bool want = true;
            if (m_Data)
                want = !m_Data.MeHidden;

            OZ_PdaFlagOp op = new OZ_PdaFlagOp();
            op.Value = want;

            string json;
            string err;
            if (!JsonFileLoader<OZ_PdaFlagOp>.MakeData(op, json, err, false))
                return true;

            OZ_Rpc.Request(OZ_PdaConst.PAGE_CONTACTS, "hide", json);
            return true;
        }

        return false;
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        // Перемикач сам нічого не несе: перепитуємо список і малюємо його.
        if (op == "hide")
        {
            if (!ok)
                SetText("ContactsHint", "#" + error);
            Request();
            return;
        }

        if (op != "list")
            return;

        if (!ok)
        {
            SetText("ContactsHint", "#" + error);
            return;
        }

        string err;
        OZ_ContactList list;
        if (!JsonFileLoader<OZ_ContactList>.LoadData(json, list, err))
        {
            OZ_Log.Error("contact list unreadable: " + err);
            return;
        }

        m_Data = list;
        Paint();
    }

    private void Paint()
    {
        if (m_List)
            m_List.ClearItems();

        int n = m_Data.Entries.Count();

        string head = "#STR_OZ_CONTACTS_ONLINE";
        head += "  " + n.ToString();
        SetText("ContactsHeader", head);

        if (m_Data.MeHidden)
            SetText("BtnHideText", "#STR_OZ_CONTACTS_SHOW_ME");
        else
            SetText("BtnHideText", "#STR_OZ_CONTACTS_HIDE_ME");

        for (int i = 0; i < n; i++)
        {
            OZ_ContactEntry e = m_Data.Entries[i];

            string row = e.Name;
            if (e.Me)
            {
                row += "   (you";
                // Невидимка бачить сама себе -- і мусить бачити, що вона
                // невидимка, інакше стан не видно ніде.
                if (m_Data.MeHidden)
                    row += ", hidden";
                row += ")";
            }

            if (m_List)
                m_List.AddItem(row, NULL, 0);
        }

        // Один у списку -- це ти сам, і сказати про це треба словами: порожній
        // на вигляд список і сервер без людей -- різні речі.
        if (n <= 1)
            SetText("ContactsHint", "#STR_OZ_CONTACTS_ALONE");
        else
            SetText("ContactsHint", "");
    }
}
