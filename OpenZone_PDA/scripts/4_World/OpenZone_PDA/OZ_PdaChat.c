// Сторінка «Зв'язок»: особисті й групові розмови.
//
// ДЖЕРЕЛО ПРАВДИ -- DISCORD. Сервер розмов не тримає взагалі: ні файлів, ні
// пам'яті. Він перекладає прохання гравця мостові й віддає назад те, що
// відповів Discord.
//
// Найважливіше наслідок: власне повідомлення НЕ з'являється в розмові
// одразу. Воно йде в Discord, і в розмову його вносить ЕХО, яке приїжджає
// довгим опитом за кілька мілісекунд. Це не затримка, яку треба обійти
// оптимістичним показом -- це і є визначення «Discord є правдою». Показати
// рядок раніше означало б показати те, чого в розмові ще немає, а якщо
// Discord його не прийме -- то й не буде.
//
// ХТО КОМУ МОЖЕ ПИСАТИ вирішує СЕРВЕР, а не міст. Особисту розмову можна
// почати лише з КОНТАКТОМ -- тим, з ким уже потиснули руки (див.
// OZ_PdaContacts). Це не обмеження заради обмеження: без нього кожен міг би
// написати кожному, знаючи лише ім'я. Розмови належать Discord, але право
// їх заводити -- ігровій механіці, і воно лишається тут.
//
// ВІДПОВІДІ ВІДКЛАДЕНІ. RestContext асинхронний, тож Handle() не має чого
// повернути: він каже OZ_Const.DEFER і відповідає сам, коли міст озветься.

// ------------------------------------------------------- листи до моста

class OZ_ChatAskMine
{
    string Uid;
}

class OZ_ChatAskOpen
{
    string Uid;
    string Id;
    int    Limit;
}

class OZ_ChatAskOlder
{
    string Uid;
    string Id;
    string Before;
    int    Limit;
}

class OZ_ChatAskSend
{
    string Uid;
    string Name;
    string Id;
    string Text;
    bool   Anon;
}

class OZ_ChatAskStart
{
    string Uid;
    string Name;
    string OtherUid;
    string OtherName;
}

class OZ_ChatAskGroup
{
    string Uid;
    string Title;
    string Desc;
}

// Правка існуючої групи: назва й опис. Порожнє поле -- «не чіпати» вирішує
// міст, який єдиний знає поточні значення.
class OZ_ChatAskGroupEdit
{
    string Uid;
    string Id;
    string Title;
    string Desc;
}

class OZ_ChatAskInvite
{
    string Uid;
    string Id;
    string OtherUid;
}

// Відмова моста. Код -- машинний («no_chat»), а не готове речення: мова
// гравця відома лише клієнтові, і рядки для неї лежать у stringtable.
class OZ_ChatFail
{
    string Error;

    static string KeyOf(string code)
    {
        if (code == "no_chat")
            return "STR_OZ_ERR_NO_CHAT";
        if (code == "not_group")
            return "STR_OZ_ERR_NOT_GROUP";
        if (code == "already_in")
            return "STR_OZ_ERR_GROUP_ALREADY_IN";
        if (code == "discord_down")
            return "STR_OZ_ERR_NO_BRIDGE";
        if (code == "read_only")
            return "STR_OZ_ERR_READ_ONLY";
        return "STR_OZ_ERR_INTERNAL";
    }
}

// Рядок, який приїхав опитом. Uid тут -- сам одержувач, тому клієнтові його
// віддавати не шкода: свій же Steam64 він і так знає.
class OZ_ChatPush
{
    string Uid;
    string Id;
    string At;
    string Who;
    string Text;
    bool   Mine;
}

// ------------------------------------------------------------ адресат

class OZ_ChatWho
{
    // Особу шукаємо серед тих, хто ЗАРАЗ на сервері, а не тримаємо посилання
    // з моменту запиту: поки відповідь їхала до Discord і назад, гравець міг
    // вийти, і збережена PlayerIdentity вказувала б у порожнечу.
    static PlayerIdentity Online(string uid)
    {
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            Man m = players[i];
            if (!m)
                continue;

            PlayerIdentity id = m.GetIdentity();
            if (id && id.GetPlainId() == uid)
                return id;
        }

        return NULL;
    }
}

// ------------------------------------------------------------ відповіді

class OZ_ChatReply : OZ_BridgeReply
{
    protected string m_Uid;
    protected string m_Op;
    protected bool   m_Body;

    // body=true -- віддати клієнтові тіло відповіді як є. Форма, якою
    // говорить міст, і форма, якої чекає сторінка, збігаються навмисно:
    // перекладати їх туди-сюди означало б тримати два описи одного й того ж.
    void OZ_ChatReply(string uid, string op, bool body)
    {
        m_Uid  = uid;
        m_Op   = op;
        m_Body = body;
    }

    override void OnBody(string json)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_ChatFail fail;
        string err;
        if (JsonFileLoader<OZ_ChatFail>.LoadData(json, fail, err) && fail && fail.Error != "")
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_CHAT, m_Op, false, "", OZ_ChatFail.KeyOf(fail.Error));
            return;
        }

        string body = "";
        if (m_Body)
            body = json;

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_CHAT, m_Op, true, body, "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_CHAT, m_Op, false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}

// Вхідні рядки з Discord. Один конверт -- один одержувач: міст уже розклав
// розмову по її учасниках, і сервер лише доносить.
//
// Воріт пристрою тут НАВМИСНО немає -- як і на самих операціях сторінки:
// розмови належать акаунту (та сама доктрина, що в записок), а показ рядка
// гейтить клієнт наявністю ввімкненого КПК. Серверні ворота на пуш нічого
// не захистили б, поки list чесно віддає той самий вміст за запитом.
class OZ_ChatSink : OZ_BridgeSink
{
    override void Deliver(string json)
    {
        OZ_ChatPush p;
        string err;
        if (!JsonFileLoader<OZ_ChatPush>.LoadData(json, p, err) || !p)
        {
            OZ_Log.Warn("chat: unreadable line from the bridge: " + err);
            return;
        }

        PlayerIdentity to = OZ_ChatWho.Online(p.Uid);
        if (!to)
            return;

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_CHAT, "line", true, json, "");
    }
}

// -------------------------------------------------------------- сторінка

class OZ_PdaHandlerChat : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok    = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (!OZ_BridgeClient.IsRunning())
        {
            error = "STR_OZ_ERR_NO_BRIDGE";
            return "";
        }

        if (op == "list")
            return List(sender, error);

        if (op == "open")
            return Open(json, sender, error);

        if (op == "older")
            return Older(json, sender, error);

        if (op == "send")
            return Send(json, sender, error);

        if (op == "start")
            return Start(json, sender, error);

        if (op == "group_new")
            return GroupNew(json, sender, error);

        if (op == "group_add")
            return GroupAdd(json, sender, error);

        if (op == "group_edit")
            return GroupEdit(json, sender, error);

        if (op == "group_del")
            return GroupDel(json, sender, error);

        if (op == "invitees")
            return Invitees(sender, ok, error);

        return "";
    }

    // ------------------------------------------------------------ читання

    private string List(PlayerIdentity sender, out string error)
    {
        string uid = sender.GetPlainId();
        string err;

        OZ_ChatAskMine a = new OZ_ChatAskMine();
        a.Uid = uid;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskMine>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/list", letter, new OZ_ChatReply(uid, "list", true));

        error = OZ_Const.DEFER;
        return "";
    }

    private string Open(string json, PlayerIdentity sender, out string error)
    {
        OZ_ChatRef r;
        string err;
        if (!JsonFileLoader<OZ_ChatRef>.LoadData(json, r, err) || !r)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string uid = sender.GetPlainId();

        OZ_ChatAskOpen a = new OZ_ChatAskOpen();
        a.Uid   = uid;
        a.Id    = r.Id;
        a.Limit = OZ_PdaConst.CHAT_KEEP;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskOpen>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/open", letter, new OZ_ChatReply(uid, "open", true));

        error = OZ_Const.DEFER;
        return "";
    }

    private string Older(string json, PlayerIdentity sender, out string error)
    {
        OZ_ChatOlderReq r;
        string err;
        if (!JsonFileLoader<OZ_ChatOlderReq>.LoadData(json, r, err) || !r || r.Id == "")
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string uid = sender.GetPlainId();

        OZ_ChatAskOlder a = new OZ_ChatAskOlder();
        a.Uid    = uid;
        a.Id     = r.Id;
        a.Before = r.Before;
        a.Limit  = 50;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskOlder>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/older", letter, new OZ_ChatReply(uid, "older", true));

        error = OZ_Const.DEFER;
        return "";
    }

    // -------------------------------------------------------------- запис

    private string Send(string json, PlayerIdentity sender, out string error)
    {
        OZ_ChatSend s;
        string err;
        if (!JsonFileLoader<OZ_ChatSend>.LoadData(json, s, err) || !s)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string text = MiscGameplayFunctions.SanitizeString(s.Text);
        if (text == "")
        {
            error = "STR_OZ_ERR_EMPTY_MSG";
            return "";
        }

        text = OZ_Text.Clip(text, OZ_PdaConst.CHAT_MSG_MAX);

        string uid = sender.GetPlainId();

        OZ_ChatAskSend a = new OZ_ChatAskSend();
        a.Uid  = uid;
        a.Name = sender.GetName();
        a.Id   = s.Id;
        a.Text = text;

        // Анонімність їде далі мостові, а СЛІД лишається тут: гравцям ім'я
        // не показується ніде, але власник сервера мусить мати, куди
        // подивитись після нічного погрому в ефірі.
        a.Anon = s.Anon;
        if (s.Anon)
            OZ_Log.Info("chat: anonymous zone message from " + uid);

        string letter;
        if (!JsonFileLoader<OZ_ChatAskSend>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        // Тіла у відповіді немає навмисно: рядок з'явиться в розмові тоді,
        // коли його поверне Discord, а не коли міст підтвердить прийом.
        OZ_BridgeClient.Call("v1/chat/send", letter, new OZ_ChatReply(uid, "send", false));

        error = OZ_Const.DEFER;
        return "";
    }

    // ------------------------------------------------------- нові розмови

    private string Start(string json, PlayerIdentity sender, out string error)
    {
        OZ_NameRef r;
        string err;
        if (!JsonFileLoader<OZ_NameRef>.LoadData(json, r, err) || !r)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string uid = sender.GetPlainId();
        OZ_PlayerData me = OZ_PlayerStore.Load(uid);

        string theirUid = UidByKeyIn(me.Friends, r.Key);
        if (theirUid == "")
        {
            // Не контакт -- писати нема кому. Саме тому контакти й заводять.
            error = "STR_OZ_ERR_NOT_CONTACT";
            return "";
        }

        OZ_ChatAskStart a = new OZ_ChatAskStart();
        a.Uid       = uid;
        a.Name      = sender.GetName();
        a.OtherUid  = theirUid;
        a.OtherName = OZ_PlayerStore.Load(theirUid).Name;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskStart>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/start", letter, new OZ_ChatReply(uid, "start", true));

        error = OZ_Const.DEFER;
        return "";
    }

    private string GroupNew(string json, PlayerIdentity sender, out string error)
    {
        OZ_ChatGroupSpec r;
        string err;
        if (!JsonFileLoader<OZ_ChatGroupSpec>.LoadData(json, r, err) || !r)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string title = MiscGameplayFunctions.SanitizeString(r.Name);
        if (title == "")
            title = "group";
        title = OZ_Text.Clip(title, OZ_PdaConst.CHAT_TITLE_MAX);

        string uid = sender.GetPlainId();

        OZ_ChatAskGroup a = new OZ_ChatAskGroup();
        a.Uid   = uid;
        a.Title = title;
        a.Desc  = OZ_Text.Clip(MiscGameplayFunctions.SanitizeString(r.Desc), OZ_PdaConst.CHAT_DESC_MAX);

        string letter;
        if (!JsonFileLoader<OZ_ChatAskGroup>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/group_new", letter, new OZ_ChatReply(uid, "group_new", true));

        error = OZ_Const.DEFER;
        return "";
    }

    private string GroupEdit(string json, PlayerIdentity sender, out string error)
    {
        OZ_ChatGroupSpec r;
        string err;
        if (!JsonFileLoader<OZ_ChatGroupSpec>.LoadData(json, r, err) || !r || r.Id == "")
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string uid = sender.GetPlainId();

        OZ_ChatAskGroupEdit a = new OZ_ChatAskGroupEdit();
        a.Uid   = uid;
        a.Id    = r.Id;
        a.Title = OZ_Text.Clip(MiscGameplayFunctions.SanitizeString(r.Name), OZ_PdaConst.CHAT_TITLE_MAX);
        a.Desc  = OZ_Text.Clip(MiscGameplayFunctions.SanitizeString(r.Desc), OZ_PdaConst.CHAT_DESC_MAX);

        string letter;
        if (!JsonFileLoader<OZ_ChatAskGroupEdit>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/group_edit", letter, new OZ_ChatReply(uid, "group_edit", false));

        error = OZ_Const.DEFER;
        return "";
    }

    private string GroupDel(string json, PlayerIdentity sender, out string error)
    {
        OZ_NoteRef r;
        string err;
        if (!JsonFileLoader<OZ_NoteRef>.LoadData(json, r, err) || !r || r.Id == "")
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string uid = sender.GetPlainId();

        OZ_NotesAskDelete a = new OZ_NotesAskDelete();
        a.Uid = uid;
        a.Id  = r.Id;

        string letter;
        if (!JsonFileLoader<OZ_NotesAskDelete>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/group_del", letter, new OZ_ChatReply(uid, "group_del", false));

        error = OZ_Const.DEFER;
        return "";
    }

    // Кого МОЖНА покликати -- вирішує сервер, а не текстове поле: клієнт
    // показує цей перелік і шле вибране ім'я в group_add, як і раніше.
    private string Invitees(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PlayerData me = OZ_PlayerStore.Load(sender.GetPlainId());

        OZ_ChatInvitees inv = new OZ_ChatInvitees();
        for (int i = 0; i < me.Friends.Count(); i++)
        {
            OZ_PlayerData d = OZ_PlayerStore.Load(me.Friends[i]);
            if (d && d.Name != "")
                inv.Names.Insert(d.Name);
        }

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_ChatInvitees>.MakeData(inv, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string GroupAdd(string json, PlayerIdentity sender, out string error)
    {
        OZ_ChatAdd add;
        string err;
        if (!JsonFileLoader<OZ_ChatAdd>.LoadData(json, add, err) || !add)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string uid = sender.GetPlainId();

        // Кликати можна лише СВОГО контакта. Інакше в групу можна було б
        // затягти будь-кого, знаючи ім'я, і група стала б способом писати
        // тим, хто цього не хотів. Чи має право сам запрошувач -- звіряє
        // міст: склад розмови знає він.
        OZ_PlayerData me = OZ_PlayerStore.Load(uid);
        string theirUid = UidByNameIn(me.Friends, add.Name);
        if (theirUid == "")
        {
            error = "STR_OZ_ERR_NOT_CONTACT";
            return "";
        }

        OZ_ChatAskInvite a = new OZ_ChatAskInvite();
        a.Uid      = uid;
        a.Id       = add.Id;
        a.OtherUid = theirUid;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskInvite>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/group_add", letter, new OZ_ChatReply(uid, "group_add", false));

        error = OZ_Const.DEFER;
        return "";
    }

    // ------------------------------------------------------------ дрібне

    // Розмову ПОЧИНАЮТЬ з обраного в списку -- там є ключ, і питання «хто це»
    // не стоїть.
    private string UidByKeyIn(array<string> uids, string key)
    {
        return OZ_Names.PickIn(uids, key);
    }

    // А в групу ЗАПРОШУЮТЬ за набраним ім'ям -- ключа в людини, яку щойно
    // надрукували, взятись нема звідки.
    //
    // Тому тут ім'я лишається, але з двома правилами, яких раніше не було:
    // порожнє не збігається ні з чим (інакше воно ловило б кожного, чиє ім'я
    // ще не кешоване), і двоє однакових -- це відмова, а не перший-ліпший.
    private string UidByNameIn(array<string> uids, string name)
    {
        if (name == "")
            return "";

        string found = "";

        for (int i = 0; uids && i < uids.Count(); i++)
        {
            OZ_PlayerData d = OZ_PlayerStore.Load(uids[i]);
            if (!d || d.Name != name)
                continue;

            if (found != "")
                return "";

            found = uids[i];
        }

        return found;
    }
}
