// Сторінка «Зв'язок»: перелік розмов ліворуч, сама розмова праворуч.
//
// Нічого не перечитуємо за таймером. Рядки ПРИХОДЯТЬ самі: Discord віддає їх
// мостові, міст -- серверові довгим опитом, сервер штовхає сюди операцією
// «line». Секундне перечитування відкритої розмови означало б запит до
// Discord щосекунди на кожного, хто просто тримає КПК відкритим.
//
// ВЛАСНЕ повідомлення теж приходить луною, а не малюється одразу. Поки
// Discord його не повернув, у розмові його НЕМАЄ -- і показати його раніше
// означало б показати те, чого може й не статись.
//
// Рядок вводу НЕ чіпаємо при оновленні -- інакше набране слово зникало б
// щоразу, коли приходить чужа репліка.

class OZ_PdaPageChat : OZ_PdaPage
{
    private Widget m_List;

    // Створені рядки бесід.
    private ref array<Widget> m_HeadRows;
    private Widget m_Lines;

    // Створені рядки розмови. Знімаємо самі -- спейсер за нами не прибирає.
    private ref array<Widget> m_LineRows;
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
        m_List      = Wgt("ChatList");
        m_HeadRows  = new array<Widget>();
        m_Lines     = Wgt("ChatLines");
        m_LineRows  = new array<Widget>();
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
        // Порожньо навмисно. Розмова оновлюється тим, що приходить, а не
        // тим, що ми питаємо.
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

    // Бесіда обирається КЛІКОМ по самому рядку: списків-віджетів тут більше
    // немає, кожна розмова -- окрема кнопка.
    override bool OnPageItemSelected(Widget w, int row)
    {
        return false;
    }

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

        // Клік по рядку бесіди. Тримаємо id, а не номер: список
        // перебудовується, і номер після цього вказував би на іншу розмову.
        if (w.GetUserID() == 3)
        {
            m_OpenId = w.GetName();
            RequestOpen();
            PaintList();
            return true;
        }

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
            //
            // Саме повідомлення НЕ дописуємо: воно приїде луною з Discord,
            // як і будь-яке чуже. Дописати його тут означало б показати
            // рядок, якого в розмові ще немає.
            if (m_Input)
                m_Input.SetText("");
            SetText("ChatHint", "");
            return;
        }

        // Новий рядок із Discord. Приходить без запиту -- сервер штовхає
        // його тому, кому він адресований.
        if (op == "line")
        {
            string perr;
            OZ_ChatPush p;
            if (!JsonFileLoader<OZ_ChatPush>.LoadData(json, p, perr) || !p)
            {
                OZ_Log.Error("chat line unreadable: " + perr);
                return;
            }

            // Не в ту розмову, що відкрита: у переліку зміниться останній
            // рядок, тож перечитуємо саме перелік, а не розмову.
            if (p.Id != m_OpenId)
            {
                RequestList();
                return;
            }

            if (!m_View)
            {
                RequestOpen();
                return;
            }

            OZ_ChatLine line = new OZ_ChatLine();
            line.At   = p.At;
            line.Who  = p.Who;
            line.Text = p.Text;
            line.Mine = p.Mine;
            m_View.Lines.Insert(line);

            PaintView();
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
            return;
        }
    }

    // Одна бесіда в лівій колонці.
    private void HeadRow(OZ_ChatHead h)
    {
        if (!m_List)
            return;

        Widget w = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_chat_head.layout", m_List);
        if (!w)
            return;

        w.SetName(h.Id);
        w.SetUserID(3);   // так OnClick відрізняє бесіду від вкладки й контакту
        m_HeadRows.Insert(w);

        Widget pick = w.FindAnyWidget("HeadPick");
        if (pick)
            pick.Show(h.Id == m_OpenId);

        TextWidget t = TextWidget.Cast(w.FindAnyWidget("HeadTitle"));
        if (t)
            t.SetText(h.Title);

        TextWidget k = TextWidget.Cast(w.FindAnyWidget("HeadKind"));
        if (k)
        {
            if (h.Kind == "group")
                k.SetText("#STR_OZ_CHAT_GROUP");
            else
                k.SetText("");
        }
    }

    // Час у людському вигляді.
    //
    // На проводі -- ISO UTC ("2026-08-25T16:47:37.079Z"), і саме так воно й
    // світилось на екрані: двадцять чотири символи машинного часу поруч із
    // трьома словами повідомлення. Для розмови треба знати день і годину, а
    // не мілісекунди й часовий пояс.
    //
    // Ріжемо за позиціями, а не парсимо: формат задає міст, він сталий, і
    // розбирати дату заради двох чисел -- це чотири нових способи помилитись.
    private string Stamp(string iso)
    {
        if (iso.Length() < 16)
            return iso;

        string day   = iso.Substring(8, 2);
        string month = iso.Substring(5, 2);
        string time  = iso.Substring(11, 5);

        return day + "." + month + "  " + time;
    }

    private void ClearLines()
    {
        for (int i = 0; i < m_LineRows.Count(); i++)
        {
            if (m_LineRows[i])
                m_LineRows[i].Unlink();
        }
        m_LineRows.Clear();
    }

    // Одна репліка.
    //
    // Своє позначаємо СМУЖКОЮ й кольором імені, а не стрілкою «>». Стрілка
    // була обхідним шляхом: у TextListbox не було чим фарбувати окремий
    // рядок, і напрям доводилось малювати символом усередині тексту.
    private void LineRow(OZ_ChatLine l)
    {
        if (!m_Lines)
            return;

        Widget w = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_chat_line.layout", m_Lines);
        if (!w)
            return;

        m_LineRows.Insert(w);

        Widget mine = w.FindAnyWidget("LineMine");
        if (mine)
            mine.Show(l.Mine);

        TextWidget who = TextWidget.Cast(w.FindAnyWidget("LineWho"));
        if (who)
        {
            who.SetText(l.Who);
            if (l.Mine)
                who.SetColor(ARGB(255, 255, 122, 26));
        }

        TextWidget at = TextWidget.Cast(w.FindAnyWidget("LineAt"));
        if (at)
            at.SetText(Stamp(l.At));

        TextWidget text = TextWidget.Cast(w.FindAnyWidget("LineText"));
        if (text)
            text.SetText(l.Text);
    }

    private void PaintList()
    {
        for (int c = 0; c < m_HeadRows.Count(); c++)
        {
            if (m_HeadRows[c])
                m_HeadRows[c].Unlink();
        }
        m_HeadRows.Clear();

        int n = 0;
        if (m_Heads && m_Heads.Items)
            n = m_Heads.Items.Count();

        for (int i = 0; i < n; i++)
            HeadRow(m_Heads.Items[i]);

        if (n == 0)
            SetText("ChatHint", "#STR_OZ_CHAT_NONE");
    }

    private void PaintView()
    {
        ClearLines();

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
            LineRow(m_View.Lines[i]);

        if (n == 0)
            SetText("ChatHint", "#STR_OZ_CHAT_EMPTY");
        else
            SetText("ChatHint", "");
    }
}
