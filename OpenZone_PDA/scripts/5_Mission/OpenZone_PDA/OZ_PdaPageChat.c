// Сторінка «Зв'язок»: перелік розмов ліворуч, сама розмова праворуч.
//
// Відкриту розмову перечитуємо раз на секунду, а перелік -- ні. Причина в
// тому, що співрозмовник пише САМ, і без опитування його рядок з'явився б аж
// після наступного натискання; а от перебудовувати перелік щосекунди означало
// б збивати виділення в той момент, коли гравець у нього цілиться.
//
// Рядок вводу НЕ чіпаємо при оновленні -- інакше набране слово зникало б
// щоразу, коли приходить чужа репліка.

class OZ_PdaPageChat : OZ_PdaPage
{
    private TextListboxWidget m_List;
    private TextListboxWidget m_Lines;
    private EditBoxWidget m_Input;
    private ButtonWidget m_BtnSend;
    private ButtonWidget m_BtnGroup;
    private ButtonWidget m_BtnInvite;

    private ref OZ_ChatList m_Heads;
    private ref OZ_ChatView m_View;

    private string m_OpenId = "";

    // Куди перейти при відкритті. Ставить сторінка «Контакти», коли гравець
    // натиснув там «написати»: інакше довелось би тримати другий список
    // контактів тут, і два списки про те саме розійшлися б.
    private static string s_Wanted = "";

    static void WantChatWith(string chatId)
    {
        s_Wanted = chatId;
    }

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_chat.layout";
    }

    override void OnBuilt()
    {
        m_List      = TextListboxWidget.Cast(Wgt("ChatList"));
        m_Lines     = TextListboxWidget.Cast(Wgt("ChatLines"));
        m_Input     = EditBoxWidget.Cast(Wgt("ChatInput"));
        m_BtnSend   = ButtonWidget.Cast(Wgt("BtnSend"));
        m_BtnGroup  = ButtonWidget.Cast(Wgt("BtnGroup"));
        m_BtnInvite = ButtonWidget.Cast(Wgt("BtnInvite"));

        SetText("BtnSendText", "#STR_OZ_CHAT_SEND");
        SetText("BtnGroupText", "#STR_OZ_CHAT_NEW_GROUP");
        SetText("BtnInviteText", "#STR_OZ_CHAT_INVITE");
    }

    override void OnSelected()
    {
        if (s_Wanted != "")
        {
            m_OpenId = s_Wanted;
            s_Wanted = "";
        }

        RequestList();
        RequestOpen();
    }

    override void OnRefresh()
    {
        // Лише відкриту розмову. Перелік -- за подією.
        RequestOpen();
    }

    private void RequestList()
    {
        OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "list", "{}");
    }

    private void RequestOpen()
    {
        if (m_OpenId == "")
            return;

        OZ_ChatRef r = new OZ_ChatRef();
        r.Id = m_OpenId;

        string json;
        string err;
        if (JsonFileLoader<OZ_ChatRef>.MakeData(r, json, err, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "open", json);
    }

    override bool OnPageItemSelected(Widget w, int row)
    {
        if (!m_List || w != m_List)
            return false;

        if (m_Heads && m_Heads.Items && row >= 0 && row < m_Heads.Items.Count())
        {
            m_OpenId = m_Heads.Items[row].Id;
            RequestOpen();
        }
        return true;
    }

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

        if (w == m_BtnSend)
        {
            SendMessage();
            return true;
        }

        if (w == m_BtnGroup)
        {
            // Назву групи беремо з того ж рядка вводу: окреме поле заради
            // однієї дії з'їло б місце, яке потрібне повідомленням.
            OZ_NameRef r = new OZ_NameRef();
            if (m_Input)
                r.Name = m_Input.GetText();

            string json;
            string err;
            if (JsonFileLoader<OZ_NameRef>.MakeData(r, json, err, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "group_new", json);
            return true;
        }

        if (w == m_BtnInvite)
        {
            OZ_ChatAdd a = new OZ_ChatAdd();
            a.Id = m_OpenId;
            if (m_Input)
                a.Name = m_Input.GetText();

            string ajson;
            string aerr;
            if (JsonFileLoader<OZ_ChatAdd>.MakeData(a, ajson, aerr, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "group_add", ajson);
            return true;
        }

        return false;
    }

    private void SendMessage()
    {
        if (m_OpenId == "")
        {
            SetText("ChatHint", "#STR_OZ_CHAT_PICK");
            return;
        }

        OZ_ChatSend s = new OZ_ChatSend();
        s.Id = m_OpenId;
        if (m_Input)
            s.Text = m_Input.GetText();

        string json;
        string err;
        if (JsonFileLoader<OZ_ChatSend>.MakeData(s, json, err, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "send", json);
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        if (!ok)
        {
            SetText("ChatHint", "#" + error);
            return;
        }

        if (op == "list")
        {
            string lerr;
            OZ_ChatList heads;
            if (!JsonFileLoader<OZ_ChatList>.LoadData(json, heads, lerr))
            {
                OZ_Log.Error("chat list unreadable: " + lerr);
                return;
            }
            m_Heads = heads;
            PaintList();
            return;
        }

        if (op == "open")
        {
            string verr;
            OZ_ChatView v;
            if (!JsonFileLoader<OZ_ChatView>.LoadData(json, v, verr))
            {
                OZ_Log.Error("chat view unreadable: " + verr);
                return;
            }
            m_View = v;
            PaintView();
            return;
        }

        if (op == "send")
        {
            // Надіслали -- рядок чистимо. Інакше наступне натискання
            // повторило б те саме повідомлення.
            if (m_Input)
                m_Input.SetText("");
            SetText("ChatHint", "");
            RequestOpen();
            RequestList();
            return;
        }

        if (op == "start" || op == "group_new")
        {
            // Сервер відповів id нової розмови -- одразу її й відкриваємо.
            string rerr;
            OZ_ChatRef r;
            if (JsonFileLoader<OZ_ChatRef>.LoadData(json, r, rerr) && r)
                m_OpenId = r.Id;

            if (m_Input)
                m_Input.SetText("");

            RequestList();
            RequestOpen();
            return;
        }

        if (op == "group_add")
        {
            if (m_Input)
                m_Input.SetText("");
            SetText("ChatHint", "");
            RequestOpen();
            return;
        }
    }

    private void PaintList()
    {
        if (m_List)
            m_List.ClearItems();

        int n = 0;
        if (m_Heads && m_Heads.Items)
            n = m_Heads.Items.Count();

        int keep = -1;

        for (int i = 0; i < n; i++)
        {
            OZ_ChatHead h = m_Heads.Items[i];

            string row = h.Title;
            if (h.Kind == "group")
                row += "  [#STR_OZ_CHAT_GROUP]";

            if (m_List)
                m_List.AddItem(row, NULL, 0);

            if (h.Id == m_OpenId)
                keep = i;
        }

        if (m_List && keep != -1)
            m_List.SelectRow(keep);

        if (n == 0)
            SetText("ChatHint", "#STR_OZ_CHAT_NONE");
    }

    private void PaintView()
    {
        if (m_Lines)
            m_Lines.ClearItems();

        if (!m_View)
            return;

        SetText("ChatTitle", m_View.Title);

        // Кликати в розмову можна лише в групову: особиста розмова -- це рівно
        // двоє, і третій у ній не «запрошений», а зовсім інша розмова.
        if (m_BtnInvite)
            m_BtnInvite.Show(m_View.Kind == "group");

        int n = 0;
        if (m_View.Lines)
            n = m_View.Lines.Count();

        for (int i = 0; i < n; i++)
        {
            OZ_ChatLine l = m_View.Lines[i];

            // Своє повідомлення позначаємо стрілкою, а не кольором: колір у
            // списку задається стилем, і сперечатися з ним заради двох станів
            // не варто.
            string row = "  ";
            if (l.Mine)
                row = "> ";

            row += l.Who;
            row += ":  " + l.Text;

            if (m_Lines)
                m_Lines.AddItem(row, NULL, 0);
        }

        // Останнє видно завжди: розмова, що показує початок, а не кінець, --
        // це розмова, у якій щоразу треба прокручувати.
        if (m_Lines && n > 0)
            m_Lines.EnsureVisible(n - 1);

        if (n == 0)
            SetText("ChatHint", "#STR_OZ_CHAT_EMPTY");
        else
            SetText("ChatHint", "");
    }
}
