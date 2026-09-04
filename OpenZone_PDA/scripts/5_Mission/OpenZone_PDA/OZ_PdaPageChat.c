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

    // МІСТ ЛЕЖИТЬ -- ПИСАТИ НІКУДИ (ТЗ-2 R4.2).
    //
    // Дім чату -- бот, і поки він мовчить, рядок не має куди лягти. Черги
    // тут немає навмисно: повідомлення, яке гравець вважає надісланим, не
    // сміє загубитись мовчки, а черга без гарантії доставки -- це саме така
    // втрата. Тому поле вводу гасне, і підказка каже чому.
    private bool m_NoBridge;

    // «Сказати без імені». Живе лише в «Зоні»; стан тумблера скидається
    // при зміні розмови -- анонімність мусить бути свідомим рухом щоразу.
    private ButtonWidget m_BtnAnon;
    private bool m_Anon = false;
    private ButtonWidget m_BtnGroup;
    private ButtonWidget m_BtnInvite;
    private ButtonWidget m_BtnGroupEdit;
    private Widget m_InvitePanel;
    private TextListboxWidget m_InviteList;
    private ButtonWidget m_BtnInvCancel;
    private Widget m_GroupPanel;
    private ButtonWidget m_BtnGSave;
    private ButtonWidget m_BtnGCancel;
    private ref array<string> m_InviteNames = new array<string>();
    // Кого правимо в панелі групи: порожньо -- створюємо нову.
    private string m_GroupEditId = "";
    // Опис відкритої розмови: їде в панель правки як поточне значення.
    private string m_ViewDesc = "";
    private ButtonWidget m_BtnOlder;
    private ButtonWidget m_BtnMembers;
    private ButtonWidget m_BtnGDel;
    // Історія вантажиться сторінками: якір і прапорець від моста, а під час
    // дороги кнопка сама стає індикатором.
    private bool m_OlderBusy = false;
    // Після довантаження мотати треба ВГОРУ, до щойно прочитаного.
    private bool m_ScrollTop = false;

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
        m_BtnAnon   = ButtonWidget.Cast(Wgt("BtnAnon"));
        m_BtnGroup  = ButtonWidget.Cast(Wgt("BtnGroup"));
        m_BtnInvite = ButtonWidget.Cast(Wgt("BtnInvite"));
        m_BtnGroupEdit = ButtonWidget.Cast(Wgt("BtnGroupEdit"));
        m_BtnOlder = ButtonWidget.Cast(Wgt("BtnOlder"));
        SetText("BtnOlderText", "#STR_OZ_CHAT_OLDER");
        m_BtnMembers = ButtonWidget.Cast(Wgt("BtnMembers"));
        SetText("BtnMembersText", "#STR_OZ_CHAT_MEMBERS");
        m_BtnGDel = ButtonWidget.Cast(Wgt("BtnGDel"));
        SetText("BtnGDelText", "#STR_OZ_CHAT_DEL_GROUP");

        m_InvitePanel  = Wgt("InvitePanel");
        m_InviteList   = TextListboxWidget.Cast(Wgt("InviteList"));
        m_BtnInvCancel = ButtonWidget.Cast(Wgt("BtnInvCancel"));
        m_GroupPanel   = Wgt("GroupPanel");
        m_BtnGSave     = ButtonWidget.Cast(Wgt("BtnGSave"));
        m_BtnGCancel   = ButtonWidget.Cast(Wgt("BtnGCancel"));

        SetText("BtnGroupEditText", "#STR_OZ_CHAT_EDIT_GROUP");
        SetText("InviteHead", "#STR_OZ_CHAT_TAP_INVITE");
        SetText("BtnInvCancelText", "#STR_OZ_CHAT_CANCEL");
        SetText("BtnGSaveText", "#STR_OZ_CHAT_SAVE");
        SetText("BtnGCancelText", "#STR_OZ_CHAT_CANCEL");

        SetText("BtnSendText", "#STR_OZ_CHAT_SEND");
        SetText("BtnGroupText", "#STR_OZ_CHAT_NEW_GROUP");
        SetText("BtnInviteText", "#STR_OZ_CHAT_INVITE");
    }

    override void OnSelected()
    {
        string pending = OZ_PdaCompose.Take();
        if (pending != "" && m_Input)
        {
            m_Input.SetText(pending);
            SetHintSticky("ChatHint", "#STR_OZ_CHAT_MARK_HINT");
        }
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

    // Чи вже є ця розмова в лівій колонці.
    private bool HeadKnown(string id)
    {
        if (!m_Heads || !m_Heads.Items)
            return false;
        for (int hk = 0; hk < m_Heads.Items.Count(); hk++)
        {
            if (m_Heads.Items[hk].Id == id)
                return true;
        }
        return false;
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
    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

        // Клік по рядку бесіди. Тримаємо id, а не номер: список
        // перебудовується, і номер після цього вказував би на іншу розмову.
        if (w.GetUserID() == 3)
        {
            m_OpenId = w.GetName();

            // Накладки належать ПОПЕРЕДНІЙ розмові: запрошення, вибране в
            // одній групі, не має права полетіти в іншу.
            if (m_InvitePanel)
                m_InvitePanel.Show(false);
            if (m_GroupPanel)
                m_GroupPanel.Show(false);
            Widget mpo = Wgt("MembersPanel");
            if (mpo)
                mpo.Show(false);

            RequestOpen();
            PaintList();
            return true;
        }

        if (w == m_BtnSend)
        {
            SendMessage();
            return true;
        }

        if (w.GetUserID() == 6)
        {
            TakeMark(w.GetName().ToInt());
            return true;
        }

        if (w == m_BtnAnon)
        {
            m_Anon = !m_Anon;
            PaintAnon();
            return true;
        }

        if (w == m_BtnGroup)
        {
            // Назву групи беремо з того ж рядка вводу: окреме поле заради
            // однієї дії з'їло б місце, яке потрібне повідомленням.
            // Панель, а не сліпе поле вводу: ім'я та опис видно ДО
            // створення, і те саме вікно править існуючу групу.
            OpenGroupPanel("");
            return true;
        }

        if (w == m_BtnGroupEdit)
        {
            OpenGroupPanel(m_OpenId);
            return true;
        }

        if (w == m_BtnGSave)
        {
            OZ_ChatGroupSpec g = new OZ_ChatGroupSpec();
            g.Id = m_GroupEditId;

            EditBoxWidget gn = EditBoxWidget.Cast(Wgt("GName"));
            if (gn)
                g.Name = gn.GetText();
            EditBoxWidget gd = EditBoxWidget.Cast(Wgt("GDesc"));
            if (gd)
                g.Desc = gd.GetText();

            string gj;
            string gerr;
            if (JsonFileLoader<OZ_ChatGroupSpec>.MakeData(g, gj, gerr, false))
            {
                if (m_GroupEditId == "")
                    OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "group_new", gj);
                else
                    OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "group_edit", gj);
            }

            if (m_GroupPanel)
                m_GroupPanel.Show(false);
            return true;
        }

        if (w && (w.GetUserID() == 8 || w.GetUserID() == 9))
        {
            OZ_NoteRef ir = new OZ_NoteRef();
            ir.Id = w.GetName();

            string ij;
            string ierr;
            string iop = "invite_accept";
            if (w.GetUserID() == 9)
                iop = "invite_decline";

            if (JsonFileLoader<OZ_NoteRef>.MakeData(ir, ij, ierr, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, iop, ij);
            return true;
        }

        if (w == m_BtnGDel)
        {
            if (m_GroupEditId == "")
            {
                if (m_GroupPanel)
                    m_GroupPanel.Show(false);
                return true;
            }

            OZ_NoteRef gr = new OZ_NoteRef();
            gr.Id = m_GroupEditId;

            string dj;
            string derr;
            string gop = "group_del";
            if (!m_View || !m_View.Owner)
                gop = "group_leave";

            if (JsonFileLoader<OZ_NoteRef>.MakeData(gr, dj, derr, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, gop, dj);

            if (m_GroupPanel)
                m_GroupPanel.Show(false);
            return true;
        }

        if (w == m_BtnGCancel)
        {
            if (m_GroupPanel)
                m_GroupPanel.Show(false);
            return true;
        }

        if (w == m_BtnOlder)
        {
            if (m_OlderBusy || !m_View)
                return true;

            m_OlderBusy = true;
            SetText("BtnOlderText", "#STR_OZ_CHAT_LOADING");

            OZ_ChatOlderReq r = new OZ_ChatOlderReq();
            r.Id     = m_OpenId;
            r.Before = m_View.Before;

            string oj;
            string oerr;
            if (JsonFileLoader<OZ_ChatOlderReq>.MakeData(r, oj, oerr, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "older", oj);
            return true;
        }

        if (w == m_BtnMembers)
        {
            Widget mpp = Wgt("MembersPanel");
            if (mpp)
                mpp.Show(!mpp.IsVisible());
            return true;
        }

        if (w == m_BtnInvite)
        {
            // Не сліпий ввід, а ВИБІР: сервер каже, кого можна кликати.
            OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "invitees", "");
            return true;
        }

        if (w == m_BtnInvCancel)
        {
            if (m_InvitePanel)
                m_InvitePanel.Show(false);
            return true;
        }

        return false;
    }

    // Забрати мітку з повідомлення. Формат пише той самий мод з карти:
    // "[MARK] назва @ x z — опис". Не мітка -- клік нічого не робить.
    private void TakeMark(int idx)
    {
        if (!m_View || !m_View.Lines)
            return;
        if (idx < 0 || idx >= m_View.Lines.Count())
            return;

        string text = m_View.Lines[idx].Text;
        if (text.IndexOf("[MARK] ") != 0)
            return;

        string rest = text.Substring(7, text.Length() - 7);

        int at = rest.IndexOf(" @ ");
        if (at == -1)
            return;

        string name = rest.Substring(0, at);
        string tail = rest.Substring(at + 3, rest.Length() - at - 3);

        string desc = "";
        // Обидва написання тире: типографське зі старих рядків і ASCII з
        // нових -- EditBox губив перше разом з описом.
        int dash = tail.IndexOf(" — ");
        int dlen = 5;
        if (dash == -1)
        {
            dash = tail.IndexOf(" -- ");
            dlen = 4;
        }
        if (dash != -1)
        {
            desc = tail.Substring(dash + dlen, tail.Length() - dash - dlen);
            tail = tail.Substring(0, dash);
        }

        int sp = tail.IndexOf(" ");
        if (sp == -1)
            return;

        int px = tail.Substring(0, sp).ToInt();
        int pz = tail.Substring(sp + 1, tail.Length() - sp - 1).ToInt();
        if (px <= 0 || pz <= 0)
            return;

        OZ_MapMarker m = new OZ_MapMarker();
        m.Name = name;
        m.Desc = desc;
        m.Pos  = px.ToString() + " 0 " + pz.ToString();

        string json;
        string err;
        if (JsonFileLoader<OZ_MapMarker>.MakeData(m, json, err, false))
        {
            OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "marker_add", json);
            SetHintSticky("ChatHint", "#STR_OZ_CHAT_MARK_TAKEN");
        }
    }

    private void SendMessage()
    {
        if (m_OpenId == "")
        {
            SetText("ChatHint", "#STR_OZ_CHAT_PICK");
            return;
        }

        // Стеля -- та сама, що в сервера (їде пакетом синхронізації); понад
        // неї не шлемо взагалі, а кажемо чому (ТЗ-4 R-D1.3).
        if (m_Input && m_Input.GetText().Length() > MsgMax())
        {
            SetHintSticky("ChatHint", "#STR_OZ_ERR_MSG_TOO_LONG");
            return;
        }

        OZ_ChatSend s = new OZ_ChatSend();
        s.Id = m_OpenId;
        if (m_Input)
            s.Text = m_Input.GetText();

        if (m_View && m_View.Kind == "zone")
            s.Anon = m_Anon;

        string json;
        string err;
        if (JsonFileLoader<OZ_ChatSend>.MakeData(s, json, err, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "send", json);
    }

    // Стеля повідомлення в байтах: значення адміна з пакета синхронізації
    // (pda.msg_max), інакше поставочне.
    private int MsgMax()
    {
        int v = OZ_ClientState.Extra(OZ_PdaConst.SYNC_MSG_MAX, "").ToInt();
        if (v > 0)
            return v;
        return OZ_PdaConst.CHAT_MSG_MAX;
    }

    // Лічильник байтів під час набору (ТЗ-4 R-D1.3): рядок Length() -- це
    // байти UTF-8, і саме їх рахує сервер. Перевищення -- словом, а не
    // тихим обрізанням.
    override bool OnPageChange(Widget w, bool finished)
    {
        if (!m_Input || w != m_Input)
            return false;

        PaintCounter();
        return true;
    }

    // Лічильник живе й крізь щосекундну перемальовку: вона кличе його
    // замість того, щоб стирати підказку (зміряно на стенді -- «bytes left»
    // зникав за секунду після набору).
    private void PaintCounter()
    {
        if (!m_Input)
            return;

        int used = m_Input.GetText().Length();
        int max  = MsgMax();

        if (used == 0)
        {
            SetHint("ChatHint", "");
            return;
        }

        if (used > max)
        {
            string over = Widget.TranslateString("#STR_OZ_CHAT_OVER");
            over += " " + (used - max).ToString();
            SetHint("ChatHint", over);
            return;
        }

        string left = Widget.TranslateString("#STR_OZ_CHAT_LEFT");
        left += " " + (max - used).ToString();
        SetHint("ChatHint", left);
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        // ПЕРЕД загальними ворітьми відмов: кнопка-індикатор мусить ожити
        // й після невдачі, інакше вона навічно лишається "Loading...".
        if (op == "older")
        {
            m_OlderBusy = false;
            SetText("BtnOlderText", "#STR_OZ_CHAT_OLDER");

            if (!ok)
            {
                SetText("ChatHint", "#" + error);
                return;
            }

            OZ_ChatView older;
            string olerr;
            if (!JsonFileLoader<OZ_ChatView>.LoadData(json, older, olerr) || !older || !older.Lines)
                return;

            // Відповідь прив'язана до РОЗМОВИ: поки вона їхала, гравець міг
            // відкрити іншу, і чужа історія не має права в неї вклеїтись.
            if (!m_View || older.Id != m_OpenId)
                return;

            for (int oi = older.Lines.Count() - 1; oi >= 0; oi--)
                m_View.Lines.InsertAt(older.Lines[oi], 0);

            m_View.More   = older.More;
            m_View.Before = older.Before;

            m_ScrollTop = true;
            PaintView();
            return;
        }

        if (!ok)
        {
            // Відмова моста -- стан сторінки, а не одна невдала операція.
            // Ловимо її ТУТ, бо сюди сходяться всі відмови, і перемальовуємо:
            // саме перемальовування й гасить поле вводу.
            if (error == "STR_OZ_ERR_NO_BRIDGE")
            {
                m_NoBridge = true;
                PaintInput();
            }

            SetText("ChatHint", "#" + error);
            return;
        }

        // Відповідь дійшла -- отже міст живий. Знімаємо стан тут, а не за
        // таймером: подія краща за опитування, і вона вже є.
        m_NoBridge = false;
        PaintInput();

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
            if (m_BtnGroup)
                m_BtnGroup.Show(!heads.Frozen);
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
            // КАПСУЛА живих рядків не бачить: зріз закінчується миттю
            // заморозки, і свіжий пуш у відкриту читальню зламав би його.
            // Сервер уже не шле пуші тримачам заморожених приладів, але
            // пристрій міг замерзнути ПІД відкритою вкладкою -- ця межа
            // клієнтська.
            if (m_View && m_View.Frozen)
                return;
            if (m_Heads && m_Heads.Frozen)
                return;

            string perr;
            OZ_ChatPush p;
            if (!JsonFileLoader<OZ_ChatPush>.LoadData(json, p, perr) || !p)
            {
                OZ_Log.Error("chat line unreadable: " + perr);
                return;
            }

            // Не в ту розмову, що відкрита. Перелік перечитуємо ЛИШЕ коли
            // цієї розмови в ньому ще немає: відомій пуш нічого видимого
            // не міняє (підзаголовок -- рід розмови, не останній рядок),
            // а повний перепит на кожне повідомлення Зони масштабується
            // числом повідомлень, не кліків.
            if (p.Id != m_OpenId)
            {
                if (!HeadKnown(p.Id))
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
            line.WhoColor = p.WhoColor;
            m_View.Lines.Insert(line);

            PaintView();
            if (!HeadKnown(p.Id))
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

        if (op == "invitees")
        {
            if (!ok)
            {
                SetHintSticky("ChatHint", "#" + error);
                return;
            }

            OZ_ChatInvitees inv;
            string ierr;
            if (!JsonFileLoader<OZ_ChatInvitees>.LoadData(json, inv, ierr) || !inv || !inv.Names)
                return;

            m_InviteNames.Clear();
            if (m_InviteList)
                m_InviteList.ClearItems();

            for (int ii = 0; ii < inv.Names.Count(); ii++)
            {
                m_InviteNames.Insert(inv.Names[ii]);
                if (m_InviteList)
                {
                    int irow = m_InviteList.AddItem(inv.Names[ii], NULL, 0);
                    m_InviteList.SetItemColor(irow, 0, ARGB(255, 79, 181, 232));
                }
            }

            if (m_InviteNames.Count() == 0)
            {
                SetHintSticky("ChatHint", "#STR_OZ_CHAT_NO_CONTACTS");
                return;
            }

            if (m_InvitePanel)
                m_InvitePanel.Show(true);
            return;
        }

        if (op == "invite_accept" || op == "invite_decline")
        {
            if (!ok)
                SetHintSticky("ChatHint", "#" + error);
            RequestList();
            return;
        }

        if (op == "group_del" || op == "group_leave")
        {
            // Розмови більше немає -- і відкритої теж, якщо це була вона.
            if (m_OpenId == m_GroupEditId || m_View && m_View.Id == m_GroupEditId)
            {
                m_OpenId = "";
                m_View = null;
                ClearLines();
                SetText("ChatTitle", "");
            }
            RequestList();
            return;
        }

        if (op == "group_edit")
        {
            // І перелік, і відкриту розмову: панель правки бере поточні
            // значення з m_View, і без перечитування друге редагування
            // відкрилося б зі старими.
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

    // Запрошення до групи -- над розмовами: воно чекає на рішення, і
    // ховати його під стосом бесід означало б, що його ніхто не побачить.
    private void InviteRow(OZ_ChatInvite inv)
    {
        if (!m_List || !inv)
            return;

        Widget w = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_chat_invite.layout", m_List);
        if (!w)
            return;

        m_HeadRows.Insert(w);

        TextWidget t = TextWidget.Cast(w.FindAnyWidget("InvTitle"));
        if (t)
            t.SetText(inv.Title);

        TextWidget f = TextWidget.Cast(w.FindAnyWidget("InvFrom"));
        if (f)
            f.SetText(Widget.TranslateString("#STR_OZ_CHAT_INV_FROM") + " " + inv.From);

        // Ім'я кнопки -- ключ групи, UserID відрізняє «так» від «ні».
        //
        // 8 і 9, а НЕ 6 і 7. Шістка вже належить рядкам чату (мітка з
        // повідомлення), і гілка, що її ловить, стоїть у OnClick ВИЩЕ --
        // тобто кнопка ПРИЙНЯТИ не доходила до свого обробника ЖОДНОГО разу.
        // Натискання йшло в TakeMark(GetName().ToInt()), а ім'я в неї -- id
        // групи, тобто ToInt() давав нуль, і мітка бралася з нульового рядка.
        // ВІДХИЛИТИ працювало: сімка ні з чим не збігалась.
        Widget yes = w.FindAnyWidget("InvJoin");
        if (yes)
        {
            yes.SetName(inv.Id);
            yes.SetUserID(8);
        }
        Widget no = w.FindAnyWidget("InvDecl");
        if (no)
        {
            no.SetName(inv.Id);
            no.SetUserID(9);
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
            // Лише слово роду: опис у два рядки не влазив і різався
            // сусіднім рядком (рішення власника 2026-08-29). Повний опис
            // видно в шапці відкритої розмови.
            if (h.Kind == "npc")
                k.SetText("#STR_OZ_CHAT_PAGER");
            else if (h.Kind == "group")
                k.SetText("#STR_OZ_CHAT_GROUP");
            else if (h.Kind == "zone")
                k.SetText("#STR_OZ_CHAT_ZONE");
            else
                k.SetText("");
        }
    }

    // Тумблер «без імені»: увімкнений горить попереджувально. Це не
    // прикраса -- гравець мусить БАЧИТИ, що зараз скаже в ефір анонімно.
    private void PaintAnon()
    {
        TextWidget t = TextWidget.Cast(Wgt("BtnAnonText"));
        if (!t)
            return;

        if (m_Anon)
        {
            t.SetText("#STR_OZ_CHAT_ANON_ON");
            t.SetColor(ARGB(255, 79, 181, 232));
        }
        else
        {
            t.SetText("#STR_OZ_CHAT_ANON");
            t.SetColor(ARGB(255, 148, 166, 181));
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
        return OZ_LocalTime.Stamp(iso);
    }

    private void ClearLines()
    {
        for (int i = 0; i < m_LineRows.Count(); i++)
        {
            if (m_LineRows[i])
            {
                // The press handler was registered on the singleton in LineRow:
                // dropping the row without dropping the entry leaves a stale
                // widget key in the handler's map.
                WidgetEventHandler.GetInstance().UnregisterWidget(m_LineRows[i]);
                m_LineRows[i].Unlink();
            }
        }
        m_LineRows.Clear();
        if (m_Lines)
            m_Lines.Update();
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

        // A self-growing row is a WrapSpacer, not a Button, and OnClick does
        // not reach it: the press is caught here and routed the same way.
        WidgetEventHandler.GetInstance().RegisterOnMouseButtonDown(w, this, "OnLineDown");

        // Рядок клікабельний: одержувач мітки забирає її одним дотиком.
        w.SetUserID(6);
        w.SetName(m_LineRows.Count().ToString());
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
            // Фракційна фарба з сервера б'є навіть «своє» помаранчеве:
            // хто ти по фракції -- важливіше, ніж чий рядок.
            if (l.WhoColor != 0)
                who.SetColor(l.WhoColor);
            else if (l.Mine)
                who.SetColor(ARGB(255, 79, 181, 232));
        }

        TextWidget at = TextWidget.Cast(w.FindAnyWidget("LineAt"));
        if (at)
            at.SetText(Stamp(l.At));

        TextWidget text = TextWidget.Cast(w.FindAnyWidget("LineText"));
        if (text)
        {
            text.SetText(l.Text);

            // Мітка -- єдиний клікабельний рядок у розмові, і виглядати
            // вона мусить інакше, ніж звичайна репліка: акцентний колір
            // каже «натисни», сірий текст -- «читай».
            if (l.Text.IndexOf("[MARK] ") == 0)
                text.SetColor(ARGB(255, 79, 181, 232));
        }
    }

    // Mouse press on a message row: the row is a spacer, so this is where its
    // click arrives (see LineRow). Left button only; the rest is not ours.
    bool OnLineDown(Widget w, int x, int y, int button)
    {
        if (button != MouseState.LEFT)
            return false;
        OZ_Log.Dbg("pda chat: line row pressed " + w.GetName());
        return OnPageClick(w, x, y);
    }

    // Межа прокрутки переліку розмов -- за фактичними рядками, тим самим
    // прийомом, що й у самої розмови.
    private void ListFit()
    {
        ScrollWidget sc = ScrollWidget.Cast(Wgt("ChatListScroll"));
        if (sc)
            sc.Update();
    }

    private void PaintList()
    {
        for (int c = 0; c < m_HeadRows.Count(); c++)
        {
            if (m_HeadRows[c])
                m_HeadRows[c].Unlink();
        }
        m_HeadRows.Clear();

        int inv = 0;
        if (m_Heads && m_Heads.Invites)
            inv = m_Heads.Invites.Count();

        for (int v = 0; v < inv; v++)
            InviteRow(m_Heads.Invites[v]);

        int n = 0;
        if (m_Heads && m_Heads.Items)
            n = m_Heads.Items.Count();

        for (int i = 0; i < n; i++)
            HeadRow(m_Heads.Items[i]);

        if (n == 0 && inv == 0)
            SetText("ChatHint", "#STR_OZ_CHAT_NONE");

        // The spacer measures itself only on Update(): heads and invites were
        // just added, and the scroll must see the new height before ListFit.
        if (m_List)
            m_List.Update();

        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(ListFit, 50, false);
    }

    override bool OnPageItemSelected(Widget w, int row)
    {
        if (!m_InviteList || w != m_InviteList)
            return false;

        if (row < 0 || row >= m_InviteNames.Count())
            return true;

        OZ_ChatAdd a = new OZ_ChatAdd();
        a.Id   = m_OpenId;
        a.Name = m_InviteNames[row];

        string ajson;
        string aerr;
        if (JsonFileLoader<OZ_ChatAdd>.MakeData(a, ajson, aerr, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_CHAT, "group_add", ajson);

        if (m_InvitePanel)
            m_InvitePanel.Show(false);
        return true;
    }

    private void OpenGroupPanel(string editId)
    {
        m_GroupEditId = editId;

        string name = "";
        string desc = "";
        if (editId != "" && m_View)
        {
            name = m_View.Title;
            desc = m_ViewDesc;
        }

        EditBoxWidget gn = EditBoxWidget.Cast(Wgt("GName"));
        if (gn)
            gn.SetText(name);
        EditBoxWidget gd = EditBoxWidget.Cast(Wgt("GDesc"));
        if (gd)
            gd.SetText(desc);

        if (editId == "")
            SetText("GroupHead", "#STR_OZ_CHAT_NEW_GROUP");
        else
            SetText("GroupHead", "#STR_OZ_CHAT_EDIT_GROUP");

        if (m_BtnGDel)
        {
            m_BtnGDel.Show(editId != "");
            // Видаляє ЗАСНОВНИК, решта -- виходить: сама кнопка, інший
            // напис і інша операція.
            if (m_View && m_View.Owner)
                SetText("BtnGDelText", "#STR_OZ_CHAT_DEL_GROUP");
            else
                SetText("BtnGDelText", "#STR_OZ_CHAT_LEAVE");
        }

        if (m_GroupPanel)
            m_GroupPanel.Show(true);
    }

    // До самого низу -- ВІДКЛАДЕНО на кадр: розмір вмісту скролер рахує
    // лише в рендері (та сама сім'я, що й GetLinesCount), і виклик одразу
    // після вставки рядків мотає за СТАРОЮ висотою.
    private void ScrollDown()
    {
        ScrollWidget sc = ScrollWidget.Cast(Wgt("ChatScroll"));
        if (!sc)
            return;

        sc.Update();

        if (m_ScrollTop)
        {
            m_ScrollTop = false;
            sc.VScrollToPos01(0);
            return;
        }

        // Вниз -- завжди: висоту тепер рахує сам спейсер, і власна умова
        // «чи є куди» більше не потрібна.
        //
        // The content height is honest now (the spacer's own), so scrolling
        // to the end of content that fits is a no-op: no "scrolled into
        // nowhere" on a one-line conversation.
        sc.VScrollToPos01(1.0);
    }

    // ЧИ Є КУДИ ПИСАТИ. Окремо від PaintView, і це не дрібниця.
    //
    // PaintView виходить першим рядком, коли розмова не відкрита, -- а
    // «міст лежить» треба вимовити САМЕ тоді: гравець дивиться на список
    // розмов, і поле вводу під ним пропонує писати в нікуди. Знайдено на
    // стенді 2026-09-01: підказка вже казала «немає зв'язку», а поле й SEND
    // лишались на екрані.
    //
    // ТРИ ПРИЧИНИ, ОДНА ПОВЕРХНЯ: пейджер (канал в один бік, приймач без
    // передавача); заморожена капсула -- читальня; мертвий міст -- рядку
    // немає куди лягти. Писати не можна в жодному з трьох, тож поле й SEND
    // гаснуть однаково, а підказка каже, чому саме.
    //
    // «Розмову не відкрито» сюди НЕ входить навмисно. Поле там і до цієї
    // роботи стояло видимим, і міняти вигляд сторінки в звичайному стані --
    // не те, про що просили: ця правка про мертвий міст, а не про те, як
    // виглядає список розмов.
    private void PaintInput()
    {
        bool mute = m_NoBridge;
        if (m_View && (m_View.Kind == "npc" || m_View.Frozen))
            mute = true;

        if (m_Input)
            m_Input.Show(!mute);
        if (m_BtnSend)
            m_BtnSend.Show(!mute);

        Widget inf = Wgt("ChatInputFrame");
        if (inf)
            inf.Show(!mute);
        Widget infl = Wgt("ChatInputFill");
        if (infl)
            infl.Show(!mute);

        // Причину називаємо в порядку спадання ваги: лежачий міст важить
        // більше за все інше, бо він єдиний із трьох -- поломка, а не стан.
        //
        // ГІЛКА else ОБОВ'ЯЗКОВА. Без неї підказка залипає: міст ожив, список
        // розмов повернувся, а під ним і далі стоїть «немає зв'язку». Видно
        // на стенді 2026-09-01 -- саме так це й знайшлось.
        if (m_NoBridge)
            SetText("ChatHint", "#STR_OZ_ERR_NO_BRIDGE");
        else if (m_View && (m_View.Kind == "npc" || m_View.Frozen))
            SetText("ChatHint", "#STR_OZ_CHAT_PAGER_RO");
        else
            PaintCounter();
    }

    private void PaintView()
    {
        ClearLines();

        if (!m_View)
            return;

        SetText("ChatTitle", m_View.Title);
        m_ViewDesc = m_View.Desc;

        // Члени групи -- накладка по кнопці MEMBERS: рядки розмови ніхто
        // не пережимає, і ламатись більше нема чому.
        bool grp = (m_View.Kind == "group");
        if (m_BtnMembers)
            m_BtnMembers.Show(grp);
        if (!grp)
        {
            Widget mp = Wgt("MembersPanel");
            if (mp)
                mp.Show(false);
        }

        if (grp)
        {
            SetText("MembersHead", "#STR_OZ_CHAT_MEMBERS");
            string ms = "";
            if (m_View.Members)
            {
                for (int mi = 0; mi < m_View.Members.Count(); mi++)
                    ms += m_View.Members[mi] + "\n";
            }
            TextWidget ml = TextWidget.Cast(Wgt("MembersList"));
            if (ml)
                ml.SetText(ms);
        }

        PaintInput();

        // Кликати в розмову можна лише в групову: особиста розмова -- це рівно
        // двоє, і третій у ній не «запрошений», а зовсім інша розмова.
        if (m_BtnInvite)
            m_BtnInvite.Show(m_View.Kind == "group" && !m_View.Frozen);
        if (m_BtnGroupEdit)
            m_BtnGroupEdit.Show(m_View.Kind == "group" && !m_View.Frozen);

        if (m_BtnAnon)
        {
            m_BtnAnon.Show(m_View.Kind == "zone" && !m_View.Frozen);
            PaintAnon();
        }

        int n = 0;
        if (m_View.Lines)
            n = m_View.Lines.Count();

        for (int i = 0; i < n; i++)
            LineRow(m_View.Lines[i]);

        if (n == 0)
            SetText("ChatHint", "#STR_OZ_CHAT_EMPTY");
        else
            PaintCounter();

        // NPC-розмова -- скриптова: історію їй не «довантажують», вона
        // ВЕДЕТЬСЯ. Кнопка старого тут лише збивала б з пантелику.
        if (m_BtnOlder)
            m_BtnOlder.Show(m_View.More && m_View.Kind != "npc");

        // Перерахунок розмірів ДО мотання: спейсер міряється лише в кадрі,
        // і скролер зі старою висотою мотає в порожнечу за останнім рядком.
        if (m_Lines)
            m_Lines.Update();

        // Розмова читається знизу: свіже повідомлення мусить бути видно без
        // ручного мотання. Після довантаження історії -- навпаки, вгору, до
        // щойно прочитаного.
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(ScrollDown, 50, false);
    }
}
