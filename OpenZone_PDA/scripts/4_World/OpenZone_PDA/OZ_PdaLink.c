// Прив'язка акаунта Discord.
//
// Обидві половини цього були написані й НІКОЛИ не зустрічались: міст віддає
// /v1/link/begin і /v1/link/status від першого коміта, а гра не кликала їх
// жодного разу. Наслідок був тихий і повний -- OZ_PlayerData.DiscordId читався
// рівно раз (заради прапорця DiscordLinked) і не писався ніде, тож прапорець
// був false усе життя мода, а всі приватні треди в гільдії стояли з нулем
// учасників: запросити в них не було кого.
//
// Порядок такий:
//
//     гравець тисне «прив'язати»
//       -> сервер просить у моста посилання (v1/link/begin)
//       -> посилання їде гравцеві, він відкриває його в браузері
//       -> сервер ПОВІЛЬНО перепитує міст (v1/link/status), поки не побачить
//          «прив'язано» або поки не вийде час
//       -> DiscordId лягає у файл акаунта
//
// Чому опитування, а не push: міст не може постукати в гру -- DayZ не приймає
// вхідних з'єднань. Єдиний канал у бік гри -- довге утримання опиту, і воно
// вже зайняте чатом. Заводити туди другий рід конверта заради події, яка
// трапляється з гравцем раз у житті, дорожче за повільний опит.

class OZ_LinkAsk
{
    string Uid = "";
}



// Що видав міст: КОРОТКИЙ КОД, який гравець переписує з екрана КПК у Discord.
//
// Не посилання OAuth. Двісті символів адреси, які людина переписує очима з
// ігрового екрана в браузер, -- найгірша частина будь-якого потоку, де вона
// зустрічається. Шість символів у чат Discord -- ні.
class OZ_LinkGrant
{
    string Code         = "";
    int    ExpiresInSec = 0;
}

class OZ_LinkState
{
    bool   Linked      = false;
    string DiscordId   = "";
    string DiscordName = "";
}

// Відповідь на «дай посилання». Їде гравцеві сторінкою «Пристрій».
class OZ_LinkBeginReply : OZ_BridgeReply
{
    protected string m_Uid;

    void OZ_LinkBeginReply(string uid)
    {
        m_Uid = uid;
    }

    override void OnBody(string json)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_LinkGrant g;
        string err;
        if (!JsonFileLoader<OZ_LinkGrant>.LoadData(json, g, err) || !g || g.Code == "")
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "link", false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        // Код доїхав -- отже з цієї миті є сенс питати статус.
        OZ_PdaLink.Watch(m_Uid);

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "link", true, json, "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "link", false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}

// Відповідь на «чи вже прив'язав». Нікому не відповідає: просто записує.
// Гравець побачить зміну наступним же status -- прапорець DiscordLinked уже
// їде в OZ_PdaDeviceStatus і давно чекав, щоб хтось його виставив.
class OZ_LinkStatusReply : OZ_BridgeReply
{
    protected string m_Uid;

    void OZ_LinkStatusReply(string uid)
    {
        m_Uid = uid;
    }

    override void OnBody(string json)
    {
        OZ_LinkState st;
        string err;
        if (!JsonFileLoader<OZ_LinkState>.LoadData(json, st, err) || !st)
            return;

        if (!st.Linked)
            return;

        OZ_PdaLink.Confirm(m_Uid, st.DiscordId, st.DiscordName);
    }

    override void OnFail(int code)
    {
        // Міст ліг посеред очікування. Нічого не робимо: наступний тік
        // спитає знову, а вийде час -- знімемось самі.
    }
}

class OZ_PdaLink
{
    // Кого зараз чекаємо: uid -> час у мілісекундах, після якого припиняємо.
    private static ref map<string, int> s_Waiting;

    // Як часто перепитуємо міст і скільки терпимо.
    //
    // П'ять секунд, а не секунда: людина йде у браузер, тисне «Authorize» і
    // вертається -- швидше за п'ять секунд це не буває. Десять хвилин на все:
    // хто не встиг, натисне ще раз, і це дешевше за опит, який ніколи не
    // припиняється.
    private static const int POLL_MS = 5000;
    private static const int GIVEUP_MS = 600000;

    private static ref Timer s_Timer;
    private static ref OZ_PdaLinkTicker s_Ticker;
    private static int s_NextAt = 0;

    // Попросити в моста посилання. Відповідь піде гравцеві сама, відкладено.
    static string Begin(string uid, out string error)
    {
        if (!OZ_BridgeClient.IsRunning())
        {
            error = "STR_OZ_ERR_NO_BRIDGE";
            return "";
        }

        OZ_LinkAsk a = new OZ_LinkAsk();
        a.Uid = uid;

        string letter;
        string err;
        if (!JsonFileLoader<OZ_LinkAsk>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("link: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/link/begin", letter, new OZ_LinkBeginReply(uid));

        error = OZ_Const.DEFER;
        return "";
    }

    // Почати чекати на цього гравця.
    static void Watch(string uid)
    {
        if (uid == "")
            return;

        if (!s_Waiting)
            s_Waiting = new map<string, int>();

        s_Waiting.Set(uid, GetGame().GetTime() + GIVEUP_MS);
        EnsureTimer();
    }

    static bool IsWaiting(string uid)
    {
        if (!s_Waiting)
            return false;
        return s_Waiting.Contains(uid);
    }

    // Міст сказав «прив'язано». Пишемо у файл акаунта й перестаємо чекати.
    static void Confirm(string uid, string discordId, string discordName)
    {
        if (!GetGame().IsServer())
            return;

        // Порожній id -- це НЕ прив'язка. Раніше сюди писався сам uid гравця,
        // і поле під назвою DiscordId тримало Steam64: прапорець ставав true,
        // а значення було вигадкою. Міст тепер віддає справжній id, і без
        // нього писати нічого.
        if (discordId == "")
            return;

        OZ_PlayerData d = OZ_PlayerStore.Load(uid);

        if (d.DiscordId != discordId)
        {
            d.DiscordId = discordId;
            OZ_PlayerStore.MarkDirty(uid);

            string m = "link: " + uid;
            m += " is now linked as \"" + discordName + "\"";
            OZ_Log.Info(m);
        }

        Forget(uid);
    }

    static void Forget(string uid)
    {
        if (!s_Waiting)
            return;
        if (!s_Waiting.Contains(uid))
            return;

        s_Waiting.Remove(uid);
    }

    private static void EnsureTimer()
    {
        if (s_Timer)
            return;

        // Носія тримаємо ЖИВИМ у статичному полі: таймер зберігає на нього
        // слабке посилання, і локальний примірник тут прибрався б лічильником
        // одразу після виходу з методу, а таймер тікав би в порожнечу.
        s_Ticker = new OZ_PdaLinkTicker();

        s_Timer = new Timer(CALL_CATEGORY_SYSTEM);
        s_Timer.Run(1.0, s_Ticker, "OZ_PdaLinkTick", NULL, true);
    }

    // Кличеться раз на секунду, але СПРАВЖНЮ роботу робить раз на п'ять:
    // секундний такт тут лише щоб не заводити другий таймер під час
    // очікування й не гасити його потім.
    static void Tick()
    {
        if (!s_Waiting)
            return;
        if (s_Waiting.Count() == 0)
            return;
        if (!OZ_BridgeClient.IsRunning())
            return;

        int now = GetGame().GetTime();
        if (now < s_NextAt)
            return;
        s_NextAt = now + POLL_MS;

        // Знімаємо прострочених ОКРЕМИМ проходом: правити мапу, по якій
        // ітеруєш, -- це та помилка, яку потім ловлять місяцями.
        array<string> expired = new array<string>();

        for (int i = 0; i < s_Waiting.Count(); i++)
        {
            string uid = s_Waiting.GetKey(i);

            if (now > s_Waiting.GetElement(i))
            {
                expired.Insert(uid);
                continue;
            }

            OZ_LinkAsk a = new OZ_LinkAsk();
            a.Uid = uid;

            string letter;
            string err;
            if (!JsonFileLoader<OZ_LinkAsk>.MakeData(a, letter, err, false))
                continue;

            OZ_BridgeClient.Call("v1/link/status", letter, new OZ_LinkStatusReply(uid));
        }

        for (int j = 0; j < expired.Count(); j++)
        {
            OZ_Log.Dbg("link: gave up waiting for " + expired[j]);
            s_Waiting.Remove(expired[j]);
        }
    }
}

// Таймер Enforce кличе метод ЗА ІМЕНЕМ на об'єкті, а OZ_PdaLink -- статичний
// клас без примірника. Тому один глобальний носій: він і є той об'єкт.
class OZ_PdaLinkTicker
{
    void OZ_PdaLinkTick()
    {
        OZ_PdaLink.Tick();
    }
}
