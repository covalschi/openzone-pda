// Сторінка «Новини»: стрічка форуму, що читається з КПК.
//
// ДЖЕРЕЛО ПРАВДИ -- DISCORD-ФОРУМ «новини», і пишуть туди ЛИШЕ
// адміністратори гільдії. Гра тільки читає: список постів і тіло одного
// поста. Це той випадок, де форум доречний -- вміст публічний за задумом,
// на відміну від нотатника, якому форум не підходив саме через публічність.
//
// Відповіді відкладені, як у чату й записок: міст асинхронний.

class OZ_NewsItem
{
    string Id      = "";
    string Title   = "";
    string Who     = "";
    string At      = "";
    int    Replies = 0;
}

class OZ_NewsList
{
    ref array<ref OZ_NewsItem> Items;

    void OZ_NewsList()
    {
        Items = new array<ref OZ_NewsItem>();
    }
}

class OZ_NewsView
{
    string Id    = "";
    string Title = "";
    string Who   = "";
    string At    = "";
    string Body  = "";
}

class OZ_NewsRef
{
    string Id = "";
}

class OZ_NewsFail
{
    string Error;

    static string KeyOf(string code)
    {
        if (code == "no_post")
            return "STR_OZ_ERR_NO_POST";
        return "STR_OZ_ERR_INTERNAL";
    }
}

class OZ_NewsReply : OZ_BridgeReply
{
    protected string m_Uid;
    protected string m_Op;

    void OZ_NewsReply(string uid, string op)
    {
        m_Uid = uid;
        m_Op  = op;
    }

    override void OnBody(string json)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_NewsFail fail;
        string err;
        if (JsonFileLoader<OZ_NewsFail>.LoadData(json, fail, err) && fail && fail.Error != "")
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_NEWS, m_Op, false, "", OZ_NewsFail.KeyOf(fail.Error));
            return;
        }

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_NEWS, m_Op, true, json, "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_NEWS, m_Op, false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}

// Лист списку: {Uid}. Раніше тут їздив лист записок -- записки відв'язано
// від моста, тож у новин тепер свій конверт.
class OZ_NewsAskList
{
    string Uid;
}

// Свіжий пост із моста. Розголос: бачать УСІ, хто в Зоні, -- сторінка
// перечитає перелік, а тост подзвонить і тим, у кого меню закрите.
class OZ_NewsPush
{
    string Id;
    string Title;
    string Who;
    string At;
}

class OZ_NewsSink : OZ_BridgeSink
{
    override void Deliver(string json)
    {
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            if (!players[i])
                continue;
            PlayerIdentity id = players[i].GetIdentity();
            if (id)
                OZ_Rpc.Respond(id, OZ_PdaConst.PAGE_NEWS, "push", true, json, "");
        }
    }
}

class OZ_PdaHandlerNews : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok    = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        // Alive(), а не IsRunning(): друге лишається true при мертвому боті,
        // і новини віддавали б порожній список замість «недоступно»
        // (ТЗ-2 R4.1, та сама причина, що в OZ_PdaChat).
        if (!OZ_BridgeClient.Alive())
        {
            error = "STR_OZ_ERR_NO_BRIDGE";
            return "";
        }

        string uid = sender.GetPlainId();
        string err;
        string letter;

        if (op == "list")
        {
            OZ_NewsAskList a = new OZ_NewsAskList();
            a.Uid = uid;

            if (!JsonFileLoader<OZ_NewsAskList>.MakeData(a, letter, err, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            OZ_BridgeClient.Call("v1/news/list", letter, new OZ_NewsReply(uid, "list"));
            error = OZ_Const.DEFER;
            return "";
        }

        if (op == "open")
        {
            OZ_NewsRef r;
            if (!JsonFileLoader<OZ_NewsRef>.LoadData(json, r, err) || !r)
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            OZ_NewsRef ask = new OZ_NewsRef();
            ask.Id = r.Id;

            if (!JsonFileLoader<OZ_NewsRef>.MakeData(ask, letter, err, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            OZ_BridgeClient.Call("v1/news/open", letter, new OZ_NewsReply(uid, "open"));
            error = OZ_Const.DEFER;
            return "";
        }

        return "";
    }
}
