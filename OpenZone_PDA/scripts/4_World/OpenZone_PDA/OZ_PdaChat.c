// Сторінка «Зв'язок»: особисті й групові розмови.
//
// ЧАТ ЖИВЕ В ГРІ, а не в Discord. Міст приїде й дзеркалитиме розмови в
// приватні треди, але джерелом правди лишиться сервер. Причина проста: якщо
// чат існує ЛИШЕ в Discord, то на сервері без бота КПК -- порожня коробка, а
// мод має працювати сам по собі.
//
// ХТО КОМУ МОЖЕ ПИСАТИ. Особисту розмову можна почати лише з КОНТАКТОМ --
// тим, з ким уже потиснули руки (див. OZ_PdaContacts: контакти заводяться
// зблизька й за згодою). Це не обмеження заради обмеження: без нього кожен
// міг би написати кожному, знаючи лише ім'я, і «особиста переписка сталкерів»
// перетворилась би на дошку оголошень.
//
// ФАЙЛ НА РОЗМОВУ, а не на гравця: розмову бачать двоє (або більше), і
// тримати дві копії однієї переписки означало б, що вони розійдуться.
//
// Id розмови -- це і ім'я файлу, тому в ньому немає двокрапок: Windows їх у
// іменах не приймає, і виявилось би це вже на чужому сервері.

class OZ_ChatLog : OZ_ConfigBase
{
    string Id    = "";
    string Kind  = "direct";   // direct | group
    string Title = "";         // лише для групових

    ref array<string> Members;              // Steam64 учасників
    ref array<ref OZ_ChatMsg> Messages;

    override int LatestVersion()
    {
        return 1;
    }

    override void LoadDefaults()
    {
        Version  = LatestVersion();
        Id       = "";
        Kind     = "direct";
        Title    = "";
        Members  = new array<string>();
        Messages = new array<ref OZ_ChatMsg>();
    }

    override void Validate(out int warnings)
    {
        warnings = 0;
        if (!Members)
            Members = new array<string>();
        if (!Messages)
            Messages = new array<ref OZ_ChatMsg>();
    }
}

class OZ_ChatStore
{
    static const string DIR = OZ_Const.PROFILE_DIR + "\\chats";

    static void EnsureDir()
    {
        OZ_Json.EnsureDir(DIR);
    }

    static string PathOf(string id)
    {
        return DIR + "\\" + id + ".json";
    }

    static OZ_ChatLog Load(string id)
    {
        OZ_ChatLog c = new OZ_ChatLog();
        OZ_ConfigLoader<OZ_ChatLog>.Load(PathOf(id), "chat_" + id, c, false);
        if (c.Id == "")
            c.Id = id;
        return c;
    }

    static void Save(OZ_ChatLog c)
    {
        OZ_ConfigLoader<OZ_ChatLog>.Save(PathOf(c.Id), "chat_" + c.Id, c, false);
    }

    static bool Exists(string id)
    {
        return FileExist(PathOf(id));
    }

    // Id особистої розмови не зберігається ніде: він ОБЧИСЛЮЄТЬСЯ з двох
    // Steam64, відсортованих між собою. Тому обидва боки завжди приходять до
    // одного файлу, і жодного реєстру розмов вести не треба.
    static string DirectId(string a, string b)
    {
        string lo = a;
        string hi = b;
        if (a > b)
        {
            lo = b;
            hi = a;
        }
        return "d_" + lo + "_" + hi;
    }
}

class OZ_PdaHandlerChat : OZ_PageHandler
{
    private static int s_Seq = 0;

    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "list")
            return List(sender, ok, error);

        if (op == "open")
            return Open(json, sender, ok, error);

        if (op == "send")
            return Send(json, sender, ok, error);

        if (op == "start")
            return Start(json, sender, ok, error);

        if (op == "group_new")
            return GroupNew(json, sender, ok, error);

        if (op == "group_add")
            return GroupAdd(json, sender, ok, error);

        return "";
    }

    // ------------------------------------------------------------- перелік

    private string List(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        string myUid = sender.GetPlainId();
        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);

        // Змінна НЕ зветься out: це зарезервоване слово, і парсер лається
        // на рядок, у якому воно вжите, а не на оголошення.
        OZ_ChatList reply = new OZ_ChatList();

        // Особисті розмови -- з кожним контактом, у якого вже щось написано.
        // Порожню розмову в переліку не показуємо: перелік має відповідати на
        // «з ким я говорив», а не «з ким міг би».
        for (int i = 0; me.Friends && i < me.Friends.Count(); i++)
        {
            string id = OZ_ChatStore.DirectId(myUid, me.Friends[i]);
            if (!OZ_ChatStore.Exists(id))
                continue;

            OZ_ChatLog c = OZ_ChatStore.Load(id);
            reply.Items.Insert(MakeHead(c, myUid));
        }

        // Групові -- ті, які гравець носить у своєму файлі. Тут реєстр таки
        // потрібен: id групи не обчислюється з учасників, бо вони міняються.
        for (int g = 0; me.Chats && g < me.Chats.Count(); g++)
        {
            string gid = me.Chats[g];
            if (!OZ_ChatStore.Exists(gid))
                continue;

            OZ_ChatLog gc = OZ_ChatStore.Load(gid);
            reply.Items.Insert(MakeHead(gc, myUid));
        }

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_ChatList>.MakeData(reply, outJson, err, false))
        {
            OZ_Log.Error("chat list serialise failed: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    // НЕ Head(): це ім'я вже зайняте вище по ієрархії, і парсер лається
    // «Method 'Object' is private» -- тобто не про те, що насправді сталось.
    private OZ_ChatHead MakeHead(OZ_ChatLog c, string myUid)
    {
        OZ_ChatHead h = new OZ_ChatHead();
        h.Id    = c.Id;
        h.Kind  = c.Kind;
        h.Title = TitleFor(c, myUid);
        h.Count = c.Messages.Count();

        if (c.Messages.Count() > 0)
        {
            OZ_ChatMsg last = c.Messages[c.Messages.Count() - 1];
            h.LastAt   = last.At;
            h.LastText = last.Text;
        }

        return h;
    }

    // Назва особистої розмови -- це ім'я ІНШОГО. Своє ім'я в переліку своїх
    // же розмов не каже нічого.
    private string TitleFor(OZ_ChatLog c, string myUid)
    {
        if (c.Kind == "group")
            return c.Title;

        for (int i = 0; i < c.Members.Count(); i++)
        {
            if (c.Members[i] == myUid)
                continue;

            OZ_PlayerData d = OZ_PlayerStore.Load(c.Members[i]);
            if (d.Name != "")
                return d.Name;
            return "---";
        }
        return "---";
    }

    // ------------------------------------------------------------ читання

    private string Open(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_ChatRef r;
        string err;
        if (!JsonFileLoader<OZ_ChatRef>.LoadData(json, r, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string myUid = sender.GetPlainId();

        OZ_ChatLog c;
        if (!MineOrRefuse(r.Id, myUid, c, error))
            return "";

        OZ_ChatView v = new OZ_ChatView();
        v.Id    = c.Id;
        v.Kind  = c.Kind;
        v.Title = TitleFor(c, myUid);

        for (int i = 0; i < c.Messages.Count(); i++)
        {
            OZ_ChatMsg m = c.Messages[i];

            OZ_ChatLine line = new OZ_ChatLine();
            line.At   = m.At;
            line.Who  = m.FromName;
            line.Text = m.Text;
            line.Mine = (m.FromUid == myUid);
            v.Lines.Insert(line);
        }

        for (int k = 0; k < c.Members.Count(); k++)
        {
            OZ_PlayerData d = OZ_PlayerStore.Load(c.Members[k]);
            if (d.Name != "")
                v.Members.Insert(d.Name);
        }

        string outJson;
        string oerr;
        if (!JsonFileLoader<OZ_ChatView>.MakeData(v, outJson, oerr, false))
        {
            OZ_Log.Error("chat view serialise failed: " + oerr);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    // Читати й писати можна ЛИШЕ свої розмови. Перевіряється тут, один раз,
    // і без цього рядка id чужої розмови був би ключем до чужої переписки --
    // а id особистої розмови обчислюється з двох Steam64, тобто вгадується.
    private bool MineOrRefuse(string id, string myUid, out OZ_ChatLog c, out string error)
    {
        if (id == "" || !OZ_ChatStore.Exists(id))
        {
            error = "STR_OZ_ERR_NO_CHAT";
            return false;
        }

        c = OZ_ChatStore.Load(id);

        if (c.Members.Find(myUid) == -1)
        {
            OZ_Log.Warn("chat " + id + " asked for by " + myUid + ", who is not in it");
            error = "STR_OZ_ERR_NO_CHAT";
            return false;
        }

        error = "";
        return true;
    }

    // -------------------------------------------------------------- запис

    private string Send(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_ChatSend s;
        string err;
        if (!JsonFileLoader<OZ_ChatSend>.LoadData(json, s, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string myUid = sender.GetPlainId();

        OZ_ChatLog c;
        if (!MineOrRefuse(s.Id, myUid, c, error))
            return "";

        string text = MiscGameplayFunctions.SanitizeString(s.Text);
        if (text == "")
        {
            error = "STR_OZ_ERR_EMPTY_MSG";
            return "";
        }

        if (text.Length() > OZ_PdaConst.CHAT_MSG_MAX)
            text = text.Substring(0, OZ_PdaConst.CHAT_MSG_MAX);

        OZ_ChatMsg m = new OZ_ChatMsg();
        m.At       = OZ_Time.NowUtc();
        m.FromUid  = myUid;
        m.FromName = sender.GetName();
        m.Text     = text;

        c.Messages.Insert(m);

        // Хвіст обрізаємо ЗВЕРХУ: старе першим. Файл розмови інакше росте
        // без стелі, а прочитати переписку річної давності однаково нікому
        // не спаде на думку.
        while (c.Messages.Count() > OZ_PdaConst.CHAT_KEEP)
            c.Messages.Remove(0);

        OZ_ChatStore.Save(c);

        ok = true;
        error = "";
        return "";
    }

    // ------------------------------------------------------- нові розмови

    private string Start(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_NameRef r;
        string err;
        if (!JsonFileLoader<OZ_NameRef>.LoadData(json, r, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string myUid = sender.GetPlainId();
        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);

        string theirUid = UidByNameIn(me.Friends, r.Name);
        if (theirUid == "")
        {
            // Не контакт -- писати нема кому. Саме тому контакти й заводять.
            error = "STR_OZ_ERR_NOT_CONTACT";
            return "";
        }

        string id = OZ_ChatStore.DirectId(myUid, theirUid);

        OZ_ChatLog c = OZ_ChatStore.Load(id);
        c.Id   = id;
        c.Kind = "direct";
        if (c.Members.Find(myUid) == -1)
            c.Members.Insert(myUid);
        if (c.Members.Find(theirUid) == -1)
            c.Members.Insert(theirUid);
        OZ_ChatStore.Save(c);

        ok = true;
        error = "";
        return IdReply(id);
    }

    private string GroupNew(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_NameRef r;
        string err;
        if (!JsonFileLoader<OZ_NameRef>.LoadData(json, r, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string title = MiscGameplayFunctions.SanitizeString(r.Name);
        if (title == "")
            title = "group";
        if (title.Length() > OZ_PdaConst.CHAT_TITLE_MAX)
            title = title.Substring(0, OZ_PdaConst.CHAT_TITLE_MAX);

        string myUid = sender.GetPlainId();

        s_Seq++;
        string id = "g_" + myUid;
        id += "_" + s_Seq.ToString();

        OZ_ChatLog c = new OZ_ChatLog();
        c.LoadDefaults();
        c.Id    = id;
        c.Kind  = "group";
        c.Title = title;
        c.Members.Insert(myUid);
        OZ_ChatStore.Save(c);

        Remember(myUid, id);

        ok = true;
        error = "";
        return IdReply(id);
    }

    private string GroupAdd(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_ChatAdd a;
        string err;
        if (!JsonFileLoader<OZ_ChatAdd>.LoadData(json, a, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string myUid = sender.GetPlainId();

        OZ_ChatLog c;
        if (!MineOrRefuse(a.Id, myUid, c, error))
            return "";

        if (c.Kind != "group")
        {
            error = "STR_OZ_ERR_NOT_GROUP";
            return "";
        }

        if (c.Members.Count() >= OZ_PdaConst.CHAT_GROUP_MAX)
        {
            error = "STR_OZ_ERR_GROUP_FULL";
            return "";
        }

        // Кликати можна лише СВОГО контакта. Інакше в групу можна було б
        // затягти будь-кого, знаючи ім'я, і група стала б способом писати
        // тим, хто цього не хотів.
        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);
        string theirUid = UidByNameIn(me.Friends, a.Name);
        if (theirUid == "")
        {
            error = "STR_OZ_ERR_NOT_CONTACT";
            return "";
        }

        if (c.Members.Find(theirUid) != -1)
        {
            error = "STR_OZ_ERR_ALREADY_IN";
            return "";
        }

        c.Members.Insert(theirUid);
        OZ_ChatStore.Save(c);

        Remember(theirUid, a.Id);

        ok = true;
        error = "";
        return "";
    }

    // Групу треба ЗАПАМ'ЯТАТИ в файлі гравця: id групи не обчислюється з
    // учасників (вони міняються), тож інакше знайти її буде нічим.
    private void Remember(string uid, string chatId)
    {
        OZ_PlayerData d = OZ_PlayerStore.Load(uid);
        if (!d.Chats)
            d.Chats = new array<string>();
        if (d.Chats.Find(chatId) == -1)
            d.Chats.Insert(chatId);
        OZ_PlayerStore.MarkDirty(uid);
    }

    private string IdReply(string id)
    {
        OZ_ChatRef r = new OZ_ChatRef();
        r.Id = id;

        string json;
        string err;
        if (!JsonFileLoader<OZ_ChatRef>.MakeData(r, json, err, false))
            return "";
        return json;
    }

    private string UidByNameIn(array<string> uids, string name)
    {
        for (int i = 0; uids && i < uids.Count(); i++)
        {
            OZ_PlayerData d = OZ_PlayerStore.Load(uids[i]);
            if (d.Name == name)
                return uids[i];
        }
        return "";
    }
}
