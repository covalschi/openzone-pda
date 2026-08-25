// Сторінка «Контакти»: хто зараз у Зоні і хто тобі свій.
//
// Список приходить із сервера вже відфільтрованим: невидимок у ньому немає
// зовсім, і клієнту нема чого приховувати -- бо нема чого й отримувати.
// Лічильник рахує рівно те, що видно, і другого числа тут не буває: інакше
// різниця між ними й була б відповіддю на питання «скільки невидимок».
//
// Чужого Steam64 клієнт не бачить НІКОЛИ. Усі дії посилаються ІМЕНЕМ, а кому
// воно належить -- вирішує сервер, і серед кого шукати теж він: поруч, у
// друзях, серед запитів.

class OZ_PdaPageContacts : OZ_PdaPage
{
    private TextListboxWidget m_List;
    private ButtonWidget m_BtnHide;
    private ButtonWidget m_BtnFriend;
    private ButtonWidget m_BtnDecline;
    private ButtonWidget m_BtnMsg;

    private ref OZ_ContactList m_Data;

    // Обраний рядок. Тримаємо ІМ'Я, а не номер: список перебудовується
    // щосекунди, і номер після цього вказував би вже на іншу людину.
    private string m_Picked = "";

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_contacts.layout";
    }

    override void OnBuilt()
    {
        m_List       = TextListboxWidget.Cast(Wgt("ContactList"));
        m_BtnHide    = ButtonWidget.Cast(Wgt("BtnHide"));
        m_BtnFriend  = ButtonWidget.Cast(Wgt("BtnFriend"));
        m_BtnDecline = ButtonWidget.Cast(Wgt("BtnDecline"));
        m_BtnMsg     = ButtonWidget.Cast(Wgt("BtnMsg"));

        SetText("BtnMsgText", "#STR_OZ_CHAT_MSG");
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

    override bool OnPageItemSelected(Widget w, int row)
    {
        if (!m_List || w != m_List)
            return false;

        m_Picked = "";
        if (m_Data && m_Data.Entries && row >= 0 && row < m_Data.Entries.Count())
            m_Picked = m_Data.Entries[row].Name;

        PaintButtons();
        return true;
    }

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

        if (w == m_BtnHide)
        {
            bool want = true;
            if (m_Data)
                want = !m_Data.MeHidden;

            OZ_PdaFlagOp op = new OZ_PdaFlagOp();
            op.Value = want;

            string json;
            string err;
            if (JsonFileLoader<OZ_PdaFlagOp>.MakeData(op, json, err, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_CONTACTS, "hide", json);
            return true;
        }

        if (w == m_BtnFriend)
        {
            SendByRel();
            return true;
        }

        if (w == m_BtnDecline)
        {
            Send("friend_decline");
            return true;
        }

        if (w == m_BtnMsg)
        {
            // Просимо СТОРІНКУ ЧАТУ почати розмову й одразу переходимо на неї.
            // Другого списку контактів у чаті через це не треба -- а два
            // списки про те саме розійшлися б.
            if (m_Picked == "")
            {
                SetText("ContactsHint", "#STR_OZ_FRIEND_PICK");
                return true;
            }

            OZ_NameRef r = new OZ_NameRef();
            r.Name = m_Picked;

            string mjson;
            string merr;
            if (JsonFileLoader<OZ_NameRef>.MakeData(r, mjson, merr, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "start", mjson);

            OZ_PdaMenu menu = OZ_PdaMenu.Cast(GetGame().GetUIManager().FindMenu(OZ_PdaConst.MENU_PDA));
            if (menu)
                menu.Select(OZ_PdaConst.PAGE_CHAT);
            return true;
        }

        return false;
    }

    // Одна кнопка -- чотири різні дії, і яка саме, вирішує стан стосунків.
    // Чотири окремі кнопки, три з яких завжди сірі, читалися б гірше.
    private void SendByRel()
    {
        OZ_ContactEntry e = Picked();
        if (!e)
        {
            SetText("ContactsHint", "#STR_OZ_FRIEND_PICK");
            return;
        }

        if (e.Rel == "friend")
        {
            Send("friend_drop");
            return;
        }

        if (e.Rel == "got")
        {
            Send("friend_accept");
            return;
        }

        if (e.Rel == "sent")
        {
            // Чекаємо відповіді -- натискати нема чого, і сказати про це
            // краще, ніж мовчки нічого не зробити.
            SetText("ContactsHint", "#STR_OZ_FRIEND_WAIT");
            return;
        }

        if (!e.Near)
        {
            SetText("ContactsHint", "#STR_OZ_FRIEND_TOOFAR");
            return;
        }

        Send("friend_ask");
    }

    private void Send(string op)
    {
        if (m_Picked == "")
        {
            SetText("ContactsHint", "#STR_OZ_FRIEND_PICK");
            return;
        }

        OZ_NameRef r = new OZ_NameRef();
        r.Name = m_Picked;

        string json;
        string err;
        if (JsonFileLoader<OZ_NameRef>.MakeData(r, json, err, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_CONTACTS, op, json);
    }

    private OZ_ContactEntry Picked()
    {
        if (m_Picked == "" || !m_Data || !m_Data.Entries)
            return null;

        for (int i = 0; i < m_Data.Entries.Count(); i++)
        {
            if (m_Data.Entries[i].Name == m_Picked)
                return m_Data.Entries[i];
        }
        return null;
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        // Дії самі нічого не несуть: перепитуємо список і малюємо його.
        if (op != "list")
        {
            if (!ok)
                SetText("ContactsHint", "#" + error);
            Request();
            return;
        }

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

        int keep = -1;

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
            else
            {
                string tag = Tag(e);
                if (tag != "")
                    row += "   " + tag;
            }

            // Фракція йде після всього: вона про людину, а не про стосунки, і
            // плутати ці два повідомлення в одному рядку не варто.
            if (e.Faction != "")
                row += "   - " + e.Faction;

            if (m_List)
                m_List.AddItem(row, NULL, 0);

            if (e.Name == m_Picked)
                keep = i;
        }

        // Виділення переживає перемальовку. Інакше вибір злітав би щосекунди,
        // і натиснути кнопку встигав би лише дуже швидкий гравець.
        if (m_List && keep != -1)
            m_List.SelectRow(keep);
        else if (keep == -1)
            m_Picked = "";

        PaintButtons();

        // Один у списку -- це ти сам, і сказати про це треба словами: порожній
        // на вигляд список і сервер без людей -- різні речі.
        if (n <= 1)
            SetText("ContactsHint", "#STR_OZ_CONTACTS_ALONE");
        else
            SetText("ContactsHint", "");
    }

    private string Tag(OZ_ContactEntry e)
    {
        if (e.Rel == "friend")
            return "[#STR_OZ_TAG_FRIEND]";
        if (e.Rel == "got")
            return "[#STR_OZ_TAG_GOT]";
        if (e.Rel == "sent")
            return "[#STR_OZ_TAG_SENT]";
        if (e.Near)
            return "[#STR_OZ_TAG_NEAR]";
        return "";
    }

    private void PaintButtons()
    {
        OZ_ContactEntry e = Picked();

        // Нікого не обрано або обрано себе -- дій немає. Кнопка, яка завжди
        // відмовляє, гірша за кнопку, якої немає.
        if (!e || e.Me)
        {
            if (m_BtnFriend)
                m_BtnFriend.Show(false);
            if (m_BtnDecline)
                m_BtnDecline.Show(false);
            if (m_BtnMsg)
                m_BtnMsg.Show(false);
            return;
        }

        // Писати можна лише контакту -- саме тому контакти й заводять.
        if (m_BtnMsg)
            m_BtnMsg.Show(e.Rel == "friend");

        if (m_BtnFriend)
            m_BtnFriend.Show(true);

        if (e.Rel == "friend")
            SetText("BtnFriendText", "#STR_OZ_FRIEND_DROP");
        else if (e.Rel == "got")
            SetText("BtnFriendText", "#STR_OZ_FRIEND_ACCEPT");
        else if (e.Rel == "sent")
            SetText("BtnFriendText", "#STR_OZ_FRIEND_WAIT");
        else
            SetText("BtnFriendText", "#STR_OZ_FRIEND_ASK");

        // Відмовити можна лише на запит.
        if (m_BtnDecline)
            m_BtnDecline.Show(e.Rel == "got");
        SetText("BtnDeclineText", "#STR_OZ_FRIEND_DECLINE");
    }
}
