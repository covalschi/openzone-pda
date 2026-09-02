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
    string Until;
    string Uid;
}

class OZ_ChatAskOpen
{
    string Until;
    string Uid;
    string Id;
    int    Limit;
}

class OZ_ChatAskOlder
{
    string Until;
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

    // КЛЮЧІ ПЕРСОНАЖІВ, з яких міст робить id розмови. Steam64 для цього не
    // годиться: після пермадесу той самий акаунт -- уже інша людина, а id,
    // зроблений із пари Steam64, привів би її в чужу розмову з небіжчиком,
    // разом з усією його перепискою.
    //
    // Доставка й склад лишаються за Steam64 -- пише все-таки акаунт.
    string MyKey;
    string OtherKey;
}

class OZ_ChatAskGroup
{
    string Uid;
    string Title;
    string Desc;
    // Скільки груп цей прилад дозволяє заснувати (Limits.GroupChats профілю,
    // ТЗ-4 R-F1.6). Рахує міст -- групи живуть у нього; нуль -- без межі.
    int    Max;
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

// Видалення групи: {Uid, Id}. Раніше тут їздив лист записок -- записки
// відв'язано від моста, тож у розмов тепер свій конверт.
class OZ_ChatAskGroupDel
{
    string Uid;
    string Id;
}

// Пара для замороження/розмороження особистої розмови: контакт розірвано
// (чи відновлено) -- тред у Discord замикається (відмикається), читання
// лишається. Груп це не стосується.
class OZ_ChatAskPair
{
    string A;
    string B;
}

// Фарбування імен фракційним кольором -- на СЕРВЕРІ. Таблиця кольорів
// живе в OZ_Factions, і роздавати її клієнтові не треба: міст шле
// Steam64 автора (AUid), сервер міняє його на готовий ARGB і стирає.
class OZ_ChatColors
{
    static string EnrichView(string json)
    {
        OZ_ChatView v;
        string err;
        if (!JsonFileLoader<OZ_ChatView>.LoadData(json, v, err) || !v || !v.Lines)
            return json;

        for (int i = 0; i < v.Lines.Count(); i++)
            Paint(v.Lines[i]);

        string outJson;
        if (!JsonFileLoader<OZ_ChatView>.MakeData(v, outJson, err, false))
            return json;
        return outJson;
    }

    static void Paint(OZ_ChatLine l)
    {
        if (!l || l.AUid == "")
            return;

        // УГРУПОВАННЯ, а не базова (ТЗ-1 §5). Базова є в кожного, тож колір
        // за нею пофарбував би весь чат в один відтінок і не сказав нічого.
        string fac = OZ_Identity.Get().OrgOf(l.AUid);
        if (fac != "")
            l.WhoColor = OZ_Identity.Get().FactionColor(fac, 255);
        l.AUid = "";
    }
}

class OZ_PairFreeze
{
    static void Send(string route, string a, string b)
    {
        OZ_ChatAskPair pr = new OZ_ChatAskPair();
        pr.A = a;
        pr.B = b;

        string letter;
        string err;
        if (!JsonFileLoader<OZ_ChatAskPair>.MakeData(pr, letter, err, false))
            return;

        // Відповідь нікому не потрібна: якщо міст спить, розмова просто
        // лишиться в попередньому стані до наступної нагоди.
        OZ_BridgeClient.Call(route, letter, null);
    }
}

class OZ_ChatAskInvite
{
    // Стеля складу групи з Tuning.json; склад знає лише міст, тому межа
    // їде разом із запрошенням. 0 -- без межі.
    int Max = 0;
    // Скільки секунд живе запрошення (ТЗ-4 R-D3.3); нуль -- безстроково.
    int TtlS = 0;
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
        if (code == "groups_full")
            return "STR_OZ_ERR_GROUPS_FULL";
        if (code == "not_owner")
            return "STR_OZ_ERR_NOT_OWNER";
        if (code == "group_full")
            return "STR_OZ_ERR_GROUP_FULL";
        return "STR_OZ_ERR_INTERNAL";
    }
}

// Рядок, який приїхав опитом. Uid тут -- сам одержувач, тому клієнтові його
// віддавати не шкода: свій же Steam64 він і так знає.
class OZ_ChatPush
{
    string AUid = "";
    int    WhoColor = 0;
    // Куди прийшов рядок: рід розмови і назва (для груп) -- тост показує
    // канал у заголовку.
    string Kind = "";
    string Title = "";
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

    // Ім'я, від якого ГОВОРИТЬ пристрій: власника сесії, а не тримача.
    // Для свого КПК це те саме ім'я; для захопленого -- імперсонація, і
    // вона навмисна: рішення власника 2026-08-28.
    static string NameOf(string accUid, PlayerIdentity sender)
    {
        OZ_PlayerData accPd = OZ_PlayerStore.Load(accUid);
        if (accPd && accPd.Name != "")
            return accPd.Name;
        return sender.GetName();
    }

    // Кому ДОНОСИТИ живі рядки акаунта. За ЧИЙ акаунт слухає гравець --
    // вирішує пристрій у руках: тримач чужого живого КПК слухає акаунт
    // ВЛАСНИКА сесії, а не свій. Інакше учасник спільної розмови отримував
    // би той самий рядок двічі -- раз за себе, раз за пристрій (зміряно
    // живим тестом 2026-08-29: дубль у відправника з чужого КПК). Капсула
    // не слухає нічого: її тримач не отримує рядків узагалі.
    static void Holders(string uid, array<PlayerIdentity> outTo)
    {
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            PlayerBase pl = PlayerBase.Cast(players[i]);
            if (!pl)
                continue;

            PlayerIdentity id = pl.GetIdentity();
            if (!id)
                continue;

            string acc = id.GetPlainId();
            OZ_PDA_Base dev = OZ_PdaLookup.HeldByPlayer(pl);

            // НЕМАЄ ПРИЛАДУ -- НЕМАЄ РЯДКІВ (ТЗ-4 R-G3.1). Живі рядки досі
            // доносились і гравцеві без КПК, і в коді це стояло як навмисне;
            // рішення власника зняло це. Виняток один -- віртуальний термінал,
            // якому адмін дозволив чат: у нього немає предмета за задумом.
            // Навантаження не росте (R-G3.2): прилад тут і так уже знайдено.
            if (!dev)
            {
                if (!OZ_PdaLookup.VirtualAllows(acc, OZ_PdaConst.PAGE_CHAT))
                    continue;
            }
            else if (dev.OZ_SessionUid() != "")
            {
                if (OZ_PdaCapsule.IsFrozen(dev))
                    continue;
                acc = dev.OZ_SessionUid();
            }

            if (acc == uid)
                outTo.Insert(id);
        }
    }
}

// Міст дренує розмови лише «онлайн»-акаунтів. Захоплений живий КПК
// говорить за власника й тоді, коли самого власника в Зоні немає, -- його
// акаунт теж мусить бути в списку, інакше тримач не побачить ані чужих
// рядків, ані еха власних відправлень.
class OZ_PdaUidProvider : OZ_BridgeUidProvider
{
    override void Fill(array<string> uids)
    {
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            PlayerBase pl = PlayerBase.Cast(players[i]);
            if (!pl)
                continue;

            OZ_PDA_Base dev = OZ_PdaLookup.HeldByPlayer(pl);
            if (!dev)
                continue;

            string acc = dev.OZ_SessionUid();
            if (acc == "" || OZ_PdaCapsule.IsFrozen(dev))
                continue;

            if (uids.Find(acc) == -1)
                uids.Insert(acc);
        }
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
        {
            body = json;
            if (m_Op == "open" || m_Op == "older")
                body = OZ_ChatColors.EnrichView(json);
        }

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

        if (p.AUid != "")
        {
            // Те саме правило, що в Paint(): колір несе угруповання.
            string pfac = OZ_Identity.Get().OrgOf(p.AUid);
            if (pfac != "")
                p.WhoColor = OZ_Identity.Get().FactionColor(pfac, 255);
            p.AUid = "";
        }

        string ejson;
        string eerr;
        if (!JsonFileLoader<OZ_ChatPush>.MakeData(p, ejson, eerr, false))
            ejson = json;

        array<PlayerIdentity> tos = new array<PlayerIdentity>();
        OZ_ChatWho.Holders(p.Uid, tos);
        for (int t = 0; t < tos.Count(); t++)
            OZ_Rpc.Respond(tos[t], OZ_PdaConst.PAGE_CHAT, "line", true, ejson, "");
    }
}

// -------------------------------------------------------------- сторінка

class OZ_PdaHandlerChat : OZ_PageHandler
{
    // ЧИЙ акаунт обслуговує запит -- вирішує ПРИСТРІЙ, і рішення живе
    // один виклик Handle: жива сесія на КПК говорить за свого власника,
    // хто б його не тримав. Захист -- пін, LOCK і LOG OUT OTHER DEVICES.
    private string m_Acc;
    private string m_Until;

    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok    = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        // Alive(), А НЕ IsRunning() -- і це та сама помилка, яку вже ловили
        // в OZ_Link.Gated.
        //
        // IsRunning() означає «опит увімкнено», і при мертвому боті лишається
        // true назавжди. Тобто саме тоді, коли ці ворота потрібні, вони
        // пропускали: сторінка йшла по дані до моста, якого немає, і гравець
        // діставав порожній список замість відповіді «недоступно».
        //
        // Сторінка зобов'язана відрізняти «порожньо» від «не знаю» (ТЗ-2
        // R4.1). Тут живе перше з двох місць, де ця різниця вимовляється.
        if (!OZ_BridgeClient.Alive())
        {
            error = "STR_OZ_ERR_NO_BRIDGE";
            return "";
        }

        m_Acc   = sender.GetPlainId();
        m_Until = "";

        OZ_PDA_Base capDev = OZ_PdaLookup.HeldBy(sender);
        if (capDev && capDev.OZ_SessionUid() != "")
        {
            m_Acc = capDev.OZ_SessionUid();

            // КАПСУЛА -- читальня зі зрізом: розмови власника віддаються
            // станом на мить заморозки (Until ріже міст, бо правда живе в
            // Discord і зріз не кешується -- кеш згорів би з рестартом), а
            // писати не можна нічого. Капсула без штампа -- без архіву:
            // зрізати їй нема по чому.
            if (OZ_PdaCapsule.IsFrozen(capDev))
            {
                m_Until = capDev.OZ_SnapshotAt();
                if (m_Until == "" || (op != "list" && op != "open" && op != "older"))
                {
                    error = "STR_OZ_ERR_FROZEN";
                    return "";
                }
            }
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

        if (op == "group_leave")
            return RefOp(json, sender, "group_leave", "v1/chat/group_leave", error);

        if (op == "invite_accept")
            return RefOp(json, sender, "invite_accept", "v1/chat/invite_accept", error);

        if (op == "invite_decline")
            return RefOp(json, sender, "invite_decline", "v1/chat/invite_decline", error);

        if (op == "invitees")
            return Invitees(sender, ok, error);

        return "";
    }

    // ------------------------------------------------------------ читання

    private string List(PlayerIdentity sender, out string error)
    {
        string uid = m_Acc;
        string err;

        OZ_ChatAskMine a = new OZ_ChatAskMine();
        a.Uid = uid;
        a.Until = m_Until;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskMine>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/list", letter, new OZ_ChatReply(sender.GetPlainId(), "list", true));

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

        string uid = m_Acc;

        OZ_ChatAskOpen a = new OZ_ChatAskOpen();
        a.Uid   = uid;
        a.Id    = r.Id;
        a.Limit = OZ_PdaTune.ChatHistoryOpen();
        a.Until = m_Until;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskOpen>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/open", letter, new OZ_ChatReply(sender.GetPlainId(), "open", true));

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

        string uid = m_Acc;

        OZ_ChatAskOlder a = new OZ_ChatAskOlder();
        a.Uid    = uid;
        a.Id     = r.Id;
        a.Before = r.Before;
        a.Limit = OZ_PdaTune.ChatHistoryPage();
        a.Until  = m_Until;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskOlder>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/older", letter, new OZ_ChatReply(sender.GetPlainId(), "older", true));

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

        // ВІДМОВА, а не мовчазне усічення (ТЗ-4 R-D1.3): рядок, який гравець
        // вважає надісланим, не має губити хвіст без жодного слова. Клієнт
        // рахує ті самі байти й відмовляє раніше; це -- межа для нечесного.
        if (text.Length() > OZ_PdaTune.ChatMsgMax())
        {
            error = "STR_OZ_ERR_MSG_TOO_LONG";
            return "";
        }

        string uid = m_Acc;

        OZ_ChatAskSend a = new OZ_ChatAskSend();
        a.Uid  = uid;
        a.Name = OZ_ChatWho.NameOf(uid, sender);
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
        OZ_BridgeClient.Call("v1/chat/send", letter, new OZ_ChatReply(sender.GetPlainId(), "send", false));

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

        string uid = m_Acc;
        OZ_PlayerData me = OZ_PlayerStore.Load(uid);

        string theirKey = UidByKeyIn(me.Friends, r.Key);
        if (theirKey == "")
        {
            // Не контакт -- писати нема кому. Саме тому контакти й заводять.
            error = "STR_OZ_ERR_NOT_CONTACT";
            return "";
        }

        // ЗАМОРОЖЕНОМУ НЕ ПИШУТЬ, і відмова тут навмисно та сама, що й для
        // будь-кого не-контакта. Пристрій того персонажа мовчить назавжди;
        // сказати про це окремими словами означало б повідомити про смерть,
        // чого КПК не робить (рішення власника 2026-08-30).
        //
        // Механічна причина не менш важлива: за цим Steam64 сьогодні живе
        // ІНША людина, і лист «старому знайомому» приїхав би саме їй.
        if (!OZ_PlayerStore.IsLive(theirKey))
        {
            error = "STR_OZ_ERR_NOT_CONTACT";
            return "";
        }

        string theirUid = OZ_PlayerStore.UidOfKey(theirKey);

        OZ_ChatAskStart a = new OZ_ChatAskStart();
        a.Uid       = uid;
        a.Name      = OZ_ChatWho.NameOf(uid, sender);
        a.OtherUid  = theirUid;
        a.OtherName = OZ_PlayerStore.Load(theirUid).Name;
        a.MyKey     = OZ_PlayerStore.KeyOf(uid);
        a.OtherKey  = theirKey;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskStart>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/start", letter, new OZ_ChatReply(sender.GetPlainId(), "start", true));

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
        title = OZ_Text.Clip(title, OZ_PdaTune.ChatTitleMax());

        string uid = m_Acc;

        OZ_ChatAskGroup a = new OZ_ChatAskGroup();
        a.Uid   = uid;
        a.Title = title;
        a.Desc  = OZ_Text.Clip(MiscGameplayFunctions.SanitizeString(r.Desc), OZ_PdaTune.ChatDescMax());

        // Межа груп -- з профілю приладу засновника (ТЗ-4 R-F1.6).
        a.Max = 0;
        OZ_PDA_Base gdev = OZ_PdaLookup.HeldBy(sender);
        if (gdev)
        {
            OZ_PdaProfile gprof = OZ_PdaProfiles.ForClass(gdev.GetType());
            if (gprof && gprof.Limits)
                a.Max = gprof.Limits.GroupChats;
        }

        string letter;
        if (!JsonFileLoader<OZ_ChatAskGroup>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/group_new", letter, new OZ_ChatReply(sender.GetPlainId(), "group_new", true));

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

        string uid = m_Acc;

        OZ_ChatAskGroupEdit a = new OZ_ChatAskGroupEdit();
        a.Uid   = uid;
        a.Id    = r.Id;
        a.Title = OZ_Text.Clip(MiscGameplayFunctions.SanitizeString(r.Name), OZ_PdaTune.ChatTitleMax());
        a.Desc  = OZ_Text.Clip(MiscGameplayFunctions.SanitizeString(r.Desc), OZ_PdaTune.ChatDescMax());

        string letter;
        if (!JsonFileLoader<OZ_ChatAskGroupEdit>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/group_edit", letter, new OZ_ChatReply(sender.GetPlainId(), "group_edit", false));

        error = OZ_Const.DEFER;
        return "";
    }

    // Три операції однієї форми -- {Uid, Id} мостові, ok назад: вихід із
    // групи і обидві відповіді на запрошення.
    private string RefOp(string json, PlayerIdentity sender, string op, string route, out string error)
    {
        OZ_NoteRef r;
        string err;
        if (!JsonFileLoader<OZ_NoteRef>.LoadData(json, r, err) || !r || r.Id == "")
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_ChatAskGroupDel a = new OZ_ChatAskGroupDel();
        a.Uid = m_Acc;
        a.Id  = r.Id;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskGroupDel>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call(route, letter, new OZ_ChatReply(sender.GetPlainId(), op, false));

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

        string uid = m_Acc;

        OZ_ChatAskGroupDel a = new OZ_ChatAskGroupDel();
        a.Uid = uid;
        a.Id  = r.Id;

        string letter;
        if (!JsonFileLoader<OZ_ChatAskGroupDel>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/group_del", letter, new OZ_ChatReply(sender.GetPlainId(), "group_del", false));

        error = OZ_Const.DEFER;
        return "";
    }

    // Кого МОЖНА покликати -- вирішує сервер, а не текстове поле: клієнт
    // показує цей перелік і шле вибране ім'я в group_add, як і раніше.
    private string Invitees(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        // ЗАПИСНИК ВЛАСНИКА СЕСІЇ, а не того, хто тримає прилад.
        //
        // Тут стояв sender.GetPlainId(), і це розходилось із group_add на
        // сусідньому екрані: перелік «кого можна покликати» будувався з
        // контактів ТОГО, ХТО ТРИМАЄ, а покликати вдавалось лише контакта
        // ВЛАСНИКА. На захопленому чужому терміналі (а він працює як термінал
        // власника -- рішення власника 2026-08-28) список показував своїх
        // друзів, і кожен вибір із нього повертав «не ваш контакт».
        OZ_PlayerData me = OZ_PlayerStore.Load(m_Acc);

        OZ_ChatInvitees inv = new OZ_ChatInvitees();
        for (int i = 0; i < me.Friends.Count(); i++)
        {
            // Заморожених у переліку немає: покликати нікуди. Вони просто
            // не з'являються серед тих, кого можна додати, -- рівно як
            // будь-хто, кого сервер не знає на ім'я.
            if (!OZ_PlayerStore.IsLive(me.Friends[i]))
                continue;

            OZ_PlayerData d = OZ_PlayerStore.Load(OZ_PlayerStore.UidOfKey(me.Friends[i]));
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

        string uid = m_Acc;

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
        a.Max      = OZ_PdaTune.ChatGroupMax();
        a.TtlS     = OZ_PdaTune.GroupInviteTtlS();

        string letter;
        if (!JsonFileLoader<OZ_ChatAskInvite>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("chat: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/chat/group_add", letter, new OZ_ChatReply(sender.GetPlainId(), "group_add", false));

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
    // Ім'я -> Steam64, серед СВОЇХ контактів. Список тримає ключі персонажів;
    // заморожені пропускаємо -- покликати того, кого вже немає, нікуди, а за
    // його Steam64 сьогодні живе інша людина.
    private string UidByNameIn(array<string> keys, string name)
    {
        if (name == "")
            return "";

        string found = "";

        for (int i = 0; keys && i < keys.Count(); i++)
        {
            if (!OZ_PlayerStore.IsLive(keys[i]))
                continue;

            string uid = OZ_PlayerStore.UidOfKey(keys[i]);

            OZ_PlayerData d = OZ_PlayerStore.Load(uid);
            if (!d || d.Name != name)
                continue;

            if (found != "")
                return "";

            found = uid;
        }

        return found;
    }
}
