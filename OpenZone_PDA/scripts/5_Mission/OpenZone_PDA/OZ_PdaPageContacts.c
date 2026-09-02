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
    private int m_Beat = 0;
    private Widget m_List;

    // Створені рядки. Тримаємо самі, бо їх треба знімати перед кожною
    // перемальовкою: спейсер не прибирає за нами.
    private ref array<Widget> m_Rows;
    // Фракційних кнопок тут БІЛЬШЕ НЕМАЄ: фракція ділить із контактами одну
    // вкладку й малює свої дії у власній половині (рішення власника
    // 2026-08-30). Дві кнопки «запросити» на одному екрані -- це два різні
    // місця для однієї думки, і одне з них завжди застаріває.
    private ButtonWidget m_BtnHide;
    private ButtonWidget m_BtnFriend;
    private ButtonWidget m_BtnMsg;

    private ref OZ_ContactList m_Data;

    // Обраний рядок. Тримаємо ІМ'Я, а не номер: список перебудовується
    // щосекунди, і номер після цього вказував би вже на іншу людину.
    // КЛЮЧ обраного, не ім'я: два однакових імені в списку -- і дія йшла в
    // першого-ліпшого з двох, а контакт без кешованого імені не обирався
    // взагалі. Ім'я лишається тим, чим і є: підписом на екрані.
    private string m_Picked = "";

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_contacts.layout";
    }

    override void OnBuilt()
    {
        // ВІДПОВІДІ НА ЛІДЕРСЬКІ ДІЇ. Без цієї підписки кожна з них -- запросив,
        // вигнав, передав лідерство, прийняв, обмінявся контактами в світі --
        // не казала гравцеві НІЧОГО: сервер чесно відповідав, ядро чесно
        // роздавало, і не було кому взяти. «Не вдалося, бо мосту немає»
        // виглядало точно так само, як «готово».
        OZ_RoleNotice.OnAnswer.Insert(OnRoleAnswer);

        m_List       = Wgt("ContactList");
        m_Rows       = new array<Widget>();
        m_BtnHide    = ButtonWidget.Cast(Wgt("BtnHide"));
        m_BtnFriend  = ButtonWidget.Cast(Wgt("BtnFriend"));
        m_BtnMsg     = ButtonWidget.Cast(Wgt("BtnMsg"));

        SetText("BtnMsgText", "#STR_OZ_CHAT_MSG");
    }

    // Сервер відповів на дію. Малюємо ЙОГО слова -- і свої ключі, і готовий
    // текст від моста -- і перепитуємо список: після вступу, вигнання чи
    // передачі лідерства він змінився.
    void OnRoleAnswer(string op, bool ok, string why)
    {
        SetHintSticky("ContactsHint", OZ_RoleNotice.Text());
        Request();
    }

    // ВІДПИСКА ОБОВ'ЯЗКОВА. Інвокер статичний, а сторінка -- ні: меню
    // закрилось, віджети Unlink-нуто, але Insert без Remove тримав би
    // сторінку живою вічно, і кожне нове відкриття меню додавало б ще одного
    // слухача. Перша ж відповідь тоді пішла б у слухача з мертвими
    // віджетами -- рівно той клас падінь, що вже коштував нам краху при
    // смерті з відкритими воротами.
    override void Unlink()
    {
        OZ_RoleNotice.OnAnswer.Remove(OnRoleAnswer);
        super.Unlink();
    }

    override void OnSelected()
    {
        // Відмова на дію, якої гравець уже не пам'ятає, ні до чого:
        // вкладку перемкнули -- тримати підказку більше нема сенсу.
        ClearHintHold();
        Request();
    }

    // Раз на секунду: люди заходять і виходять самі, і список мусить це
    // показувати без натискань.
    override void OnRefresh()
    {
        // Страховка раз на 5 секунд: живі зміни приходять пушами одразу,
        // щосекундний перепит лише дублював їх (аудит 2026-08-30).
        m_Beat++;
        if (m_Beat % 5 != 0)
            return;
        Request();
    }

    private void Request()
    {
        OZ_Rpc.Request(OZ_PdaConst.PAGE_CONTACTS, "list", "{}");
    }

    // Рядок обирається КЛІКОМ по самому рядку: віджетів-списків тут більше
    // немає, кожен контакт -- окрема кнопка.
    override bool OnPageItemSelected(Widget w, int row)
    {
        return false;
    }

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

        // Клік по рядку контакту. Тримаємо ІМ'Я, а не номер: список
        // перебудовується щосекунди, і номер після цього вказував би вже на
        // іншу людину.
        if (w.GetUserID() == 2)
        {
            m_Picked = w.GetName();   // ім'я віджета -- це ключ, див. Row()
            Paint();
            return true;
        }

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

        if (w == m_BtnMsg)
        {
            // Просимо СТОРІНКУ ЧАТУ почати розмову й одразу переходимо на неї.
            // Другого списку контактів у чаті через це не треба -- а два
            // списки про те саме розійшлися б.
            if (m_Picked == "")
            {
                SetHintSticky("ContactsHint", "#STR_OZ_FRIEND_PICK");
                return true;
            }

            OZ_NameRef r = new OZ_NameRef();
            r.Key = m_Picked;

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
            SetHintSticky("ContactsHint", "#STR_OZ_FRIEND_PICK");
            return;
        }

        // ДОДАТИ звідси більше не можна -- обмін відбувається в світі, дією з
        // приладом у руках. Тут лишилось те, для чого меню й потрібне:
        // ПРИБРАТИ зі свого записника. Викреслити людину зі списку -- твоя
        // особиста справа, і зустрічатись заради неї ні з ким не треба.
        Send("friend_drop");
    }

    private void Send(string op)
    {
        if (m_Picked == "")
        {
            SetHintSticky("ContactsHint", "#STR_OZ_FRIEND_PICK");
            return;
        }

        OZ_NameRef r = new OZ_NameRef();
        r.Key = m_Picked;

        string json;
        string err;
        if (JsonFileLoader<OZ_NameRef>.MakeData(r, json, err, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_CONTACTS, op, json);
    }

    // Кого вибрано ліворуч -- ІМЕНЕМ, для сусідньої половини вкладки.
    //
    // Фракційні дії живуть праворуч, але «покликати» стосується того, кого
    // видно в списку людей: у цьому й був сенс зводити дві сторінки в одну
    // вкладку. Ім'я, а не ключ: далі його розв'язує сервер, і чужого
    // Steam64 клієнт як не бачив, так і не бачить.
    string PickedName()
    {
        OZ_ContactEntry e = Picked();
        if (!e)
            return "";

        // СЕБЕ не віддаємо: єдина дія сусідньої половини над обраним --
        // «покликати», а покликати себе не можна. Кнопка тоді просто не
        // з'явиться, замість того щоб з'явитись і відмовити.
        if (e.Me)
            return "";

        return e.Name;
    }

    private OZ_ContactEntry Picked()
    {
        if (m_Picked == "" || !m_Data || !m_Data.Entries)
            return null;

        for (int i = 0; i < m_Data.Entries.Count(); i++)
        {
            if (m_Data.Entries[i].Key == m_Picked)
                return m_Data.Entries[i];
        }
        return null;
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        if (op == "push")
        {
            // Ролі змінились -- сервер подзвонив, перечитуємо мовчки.
            OZ_Rpc.Request(OZ_PdaConst.PAGE_CONTACTS, "list", "{}");
            return;
        }

        // Дії самі нічого не несуть: перепитуємо список і малюємо його.
        if (op != "list")
        {
            if (!ok)
                SetHintSticky("ContactsHint", "#" + error);
            Request();
            return;
        }

        if (!ok)
        {
            // ЗАБУВАЄМО показане. Сервер відмовив -- вимкнули прилад, вийняли
            // батарею, замкнувся, -- і лишити на екрані попередній список
            // означало б показувати Зону з мертвого приладу. Саме та поведінка,
            // яку гейт живлення й прибирає.
            m_Data = null;
            m_Picked = "";
            Clear();
            SetText("ContactsHeader", "");
            PaintButtons();

            SetHintSticky("ContactsHint", "#" + error);
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
        Clear();

        int n = m_Data.Entries.Count();

        string head = "#STR_OZ_CONTACTS_ONLINE";
        if (m_Data.Frozen)
            head = "#STR_OZ_DEV_SNAP_CONTACTS";
        head += "  " + n.ToString();
        SetText("ContactsHeader", head);

        // Капсула: свою присутність із чужого замороженого приладу не
        // вмикають і не вимикають.
        if (m_BtnHide)
            m_BtnHide.Show(!m_Data.Frozen);

        if (m_Data.MeHidden)
            SetText("BtnHideText", "#STR_OZ_CONTACTS_SHOW_ME");
        else
            SetText("BtnHideText", "#STR_OZ_CONTACTS_HIDE_ME");

        bool stillThere = false;

        for (int i = 0; i < n; i++)
        {
            if (Row(m_Data.Entries[i]))
                stillThere = true;
        }

        // Виділення переживає перемальовку. Інакше вибір злітав би щосекунди,
        // і натиснути кнопку встигав би лише дуже швидкий гравець.
        if (!stillThere)
            m_Picked = "";

        PaintButtons();

        // Протухла проекція має ПЕРЕВАГУ над усіма іншими підказками: усе, що
        // намальовано вище, може бути застарілим, і мовчати про це гірше, ніж
        // не сказати про самотність.
        // Запрошення -- найважливіше, що може бути на цьому екрані.
        if (m_Data.InviteFaction != "")
        {
            string inv = m_Data.InviteFrom;
            inv += "  ->  ";
            inv += m_Data.InviteFaction;
            SetHint("ContactsHint", inv);
            return;
        }

        if (m_Data.Stale)
        {
            SetHint("ContactsHint", "#STR_OZ_CONTACTS_STALE");
            return;
        }

        // Один у списку -- це ти сам, і сказати про це треба словами: порожній
        // на вигляд список і сервер без людей -- різні речі.
        if (n <= 1)
            SetHint("ContactsHint", "#STR_OZ_CONTACTS_ALONE");
        else
            SetHint("ContactsHint", "");
    }

    // Знімаємо попередні рядки. Спейсер за нами не прибирає, і без цього
    // список ріс би щосекунди.
    private void Clear()
    {
        for (int i = 0; i < m_Rows.Count(); i++)
        {
            if (m_Rows[i])
                m_Rows[i].Unlink();
        }
        m_Rows.Clear();
    }

    // Один рядок. Повертає true, якщо це саме обраний -- так виділення
    // переживає перемальовку без пошуку по індексах, яких більше немає.
    private bool Row(OZ_ContactEntry e)
    {
        if (!m_List)
            return false;

        Widget w = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_contact_row.layout", m_List);
        if (!w)
            return false;

        // Ім'ям ВІДЖЕТА робимо ключ: саме його читає OnClick, і саме він
        // мусить означати «хто це». Підпис на екрані ставиться нижче, у
        // текстовий віджет, і на впізнання не впливає.
        w.SetName(e.Key);
        w.SetUserID(2);   // так OnClick відрізняє рядок від вкладки й кнопок
        m_Rows.Insert(w);

        bool picked = (e.Key == m_Picked);

        Widget pick = w.FindAnyWidget("RowPick");
        if (pick)
            pick.Show(picked);

        // Ім'я, і одразу поруч -- чи це ти.
        string name = e.Name;
        if (e.Me)
        {
            name += "   (you";
            // Невидимка бачить сама себе -- і мусить бачити, що вона
            // невидимка, інакше стан не видно ніде.
            if (m_Data.MeHidden)
                name += ", hidden";
            name += ")";
        }
        Put(w, "RowName", name);

        // УГРУПОВАННЯ -- СВОЇМ КОЛЬОРОМ, тим самим, що й смужка ліворуч.
        // Колір приходить із реєстру бота, тож підпис і колір з одного
        // джерела. Базова фракція сюди не потрапляє: вона в кожного, і
        // кольорова позначка з неї не сказала б нічого. Її місце -- підпис
        // нижче, поруч зі сталкерським званням.
        Put(w, "RowFaction", e.Org);

        Widget chip = w.FindAnyWidget("RowChip");
        if (e.Org != "")
        {
            TextWidget ft = TextWidget.Cast(w.FindAnyWidget("RowFaction"));
            if (ft)
                ft.SetColor(e.OrgColor);

            if (chip)
            {
                chip.SetColor(e.OrgColor);
                chip.Show(true);
            }
        }
        else if (chip)
        {
            chip.Show(false);
        }

        Put(w, "RowWhere", Where(e));
        Put(w, "RowDetail", Detail(e));

        return picked;
    }

    // Де людина: поруч, у Зоні, чи взагалі немає. Одна відповідь, не три
    // прапорці в різних кутах.
    private string Where(OZ_ContactEntry e)
    {
        if (e.Me)
            return "";
        if (e.Near)
            return "#STR_OZ_TAG_NEAR";
        if (e.Online)
            return "#STR_OZ_TAG_INZONE";

        // КОЛИ ЙОГО БАЧИЛИ ОСТАННІЙ РАЗ -- замість голого «немає».
        //
        // Це те єдине, що записник справді знає про відсутнього, і воно ж
        // єдина відповідь, яку можна дати чесно: людина могла не заходити
        // тиждень, могла піти назавжди. Різниці тут немає навмисно, і саме
        // тому дата стоїть у КОЖНОГО (рішення власника 2026-08-30).
        if (e.LastSeen != "")
            return Widget.TranslateString("#STR_OZ_TAG_LASTSEEN") + " " + Day(e.LastSeen);

        return "#STR_OZ_TAG_AWAY";
    }

    // "2026-08-30 12:41:07" -> "2026-08-30". Година тут ні до чого: у Зоні
    // важить день, а повний штамп у рядок усе одно не влазить.
    private string Day(string stamp)
    {
        int at = stamp.IndexOf(" ");
        if (at == -1)
            return stamp;
        return stamp.Substring(0, at);
    }

    // Другий рядок: посада, звання, мітки -- через розділювач, а не через
    // коми. Кома читається як перелік однорідного; це три різні речі.
    private string Detail(OZ_ContactEntry e)
    {
        string line;

        // ПОСАД І ФРАКЦІЙНИХ ЗВАНЬ ТУТ НЕМАЄ (рішення власника 2026-08-30):
        // хто в угрупованні старший -- справа угруповання, і видно це в його
        // половині вкладки. Контакти кажуть, ХТО людина: ім'я, базова
        // фракція, сталкерське звання й мітки. Внутрішня драбина чужої групи
        // в записник не потрапляє навіть підписом.
        //
        // БАЗОВА ВІСЬ ОДНИМ РЯДКОМ: звання, а коли його немає -- сама назва
        // базової фракції.
        //
        // Саме так, а не «назва плюс звання через дефіс», і це вимір, а не
        // смак: підписи звань уже НЕСУТЬ базову в собі («Сталкер-новачок»,
        // «Досвідчений сталкер»), тож склейка давала «Сталкери-Сталкер-
        // новачок» -- видно на стенді 2026-09-01. Звання і є найточнішою
        // формою відповіді «хто ти в Зоні»; назва фракції потрібна лише
        // тому, кому звання ще не дали.
        if (e.Rank != "")
            line = e.Rank;
        else if (e.Base != "")
            line = e.Base;

        if (e.Traits && e.Traits.Count() > 0)
        {
            if (line != "")
                line += "   ·   ";
            line += Join(e.Traits);
        }

        return line;
    }

    private void Put(Widget row, string name, string value)
    {
        TextWidget t = TextWidget.Cast(row.FindAnyWidget(name));
        if (t)
            t.SetText(value);
    }

    private string Join(array<string> what)
    {
        string line = "";
        for (int i = 0; i < what.Count(); i++)
        {
            if (i > 0)
                line += ", ";
            line += what[i];
        }
        return line;
    }

    private void PaintButtons()
    {
        // Капсула: людей пам'ятають, а не бачать -- жодних дій над рядком.
        if (m_Data && m_Data.Frozen)
        {
            if (m_BtnFriend)
                m_BtnFriend.Show(false);
            if (m_BtnMsg)
                m_BtnMsg.Show(false);
            return;
        }

        OZ_ContactEntry e = Picked();

        // Нікого не обрано або обрано себе -- дій немає. Кнопка, яка завжди
        // відмовляє, гірша за кнопку, якої немає.
        if (!e || e.Me)
        {
            if (m_BtnFriend)
                m_BtnFriend.Show(false);
            if (m_BtnMsg)
                m_BtnMsg.Show(false);
            return;
        }

        // Писати можна лише контакту -- саме тому контакти й заводять.
        if (m_BtnMsg)
            m_BtnMsg.Show(true);

        // Одна дія на обраному: прибрати. Додавання пішло в світ, а кнопка,
        // якої немає, чесніша за кнопку, яка завжди відмовляє.
        if (m_BtnFriend)
            m_BtnFriend.Show(true);
        SetText("BtnFriendText", "#STR_OZ_CONTACT_DROP");
    }
}
