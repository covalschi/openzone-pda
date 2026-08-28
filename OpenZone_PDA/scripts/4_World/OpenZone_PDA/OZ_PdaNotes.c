// Сторінка «Записки»: приватні нотатки гравця.
//
// ДЖЕРЕЛО ПРАВДИ -- DISCORD, як і в чату. Рішення власника 2026-08-28:
// перша редакція тримала записки файлом сервера, і це БУЛО самовільним
// відступом від спеки тонкого клієнта -- спека прописувала переїзд, а
// сесія лишила файли, не спитавши. Повертаємо як задумано.
//
// У бота на гравця -- приватний тред «Нотатник», одна записка -- одне
// повідомлення. Правок повідомлень немає взагалі: зберегти -- це видалити
// старе повідомлення й запостити нове (пост вебхука ще й безкоштовно
// розархівує тред). Id записки карбує МІСТ і він СВІЙ, не id повідомлення:
// id повідомлення змінюється кожним перепостом.
//
// Наслідки, які сторінка вже вміє з часів чату:
//   -- без моста записок НЕМАЄ. Це те саме правило, що для розмов:
//      немає Discord -- немає функціональності;
//   -- відповіді ВІДКЛАДЕНІ (OZ_Const.DEFER): RestContext асинхронний,
//      Handle() відповідає сам, коли міст озветься.
//
// Записки належать АКАУНТУ, а не пристрою: міст віддає їх по Uid, тож
// чужий КПК своїх записок не покаже, а смерть і згублений прилад їх не
// чіпають. Відсічка пристрою на них не поширюється -- і не мусить.

// ------------------------------------------------------- листи до моста

class OZ_NotesAskList
{
    string Uid;
}

class OZ_NotesAskSave
{
    string Uid;
    string Id;
    string Title;
    string Body;
    string Name;
}

class OZ_NotesAskDelete
{
    string Uid;
    string Id;
}

// Відмова моста. Код машинний, рядок для гравця добирає клієнт.
class OZ_NotesFail
{
    string Error;

    static string KeyOf(string code)
    {
        if (code == "no_note")
            return "STR_OZ_ERR_NO_NOTE";
        if (code == "notes_full")
            return "STR_OZ_ERR_NOTES_FULL";
        if (code == "discord_down")
            return "STR_OZ_ERR_NO_BRIDGE";
        return "STR_OZ_ERR_INTERNAL";
    }
}

// Книжка цілком -- формат відповіді list. Той самий клас читала стара
// файлова версія, тож клієнт розбирає обидві епохи одним описом.
class OZ_NoteBook
{
    int Version = 1;
    // Стеля книжки, як її знає міст. 0 у старих знімках на чипах -- тоді
    // читач бере власну OZ_PdaConst.NOTES_MAX як запасну.
    int Max = 0;
    ref array<ref OZ_Note> Notes;

    void OZ_NoteBook()
    {
        Notes = new array<ref OZ_Note>();
    }
}

// ------------------------------------------------------------ відповіді

class OZ_NotesReply : OZ_BridgeReply
{
    protected string m_Uid;
    protected string m_Op;
    protected bool   m_Body;

    void OZ_NotesReply(string uid, string op, bool body)
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

        OZ_NotesFail fail;
        string err;
        if (JsonFileLoader<OZ_NotesFail>.LoadData(json, fail, err) && fail && fail.Error != "")
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_NOTES, m_Op, false, "", OZ_NotesFail.KeyOf(fail.Error));
            return;
        }

        string body = "";
        if (m_Body)
            body = json;

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_NOTES, m_Op, true, body, "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_NOTES, m_Op, false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}

// -------------------------------------------------------------- сторінка

class OZ_PdaHandlerNotes : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok    = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        // Експорт на носій -- ЛОКАЛЬНИЙ: тіло записки клієнт має з list і
        // шле сам, чип лежить у слоті. Міст цій операції не потрібен, тому
        // вона стоїть ПЕРЕД ворітьми моста.
        if (op == "carrier_add")
            return CarrierAdd(json, sender, ok, error);

        if (!OZ_BridgeClient.IsRunning())
        {
            error = "STR_OZ_ERR_NO_BRIDGE";
            return "";
        }

        if (op == "list")
            return List(sender, error);

        if (op == "save")
            return Save(json, sender, error);

        if (op == "delete")
            return Delete(json, sender, error);

        return "";
    }

    // Одна записка на чип, за вибором гравця. Повторний експорт тієї самої
    // (той самий Id) оновлює її на чипі, а не плодить копію.
    private string CarrierAdd(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_Note n;
        string err;
        if (!JsonFileLoader<OZ_Note>.LoadData(json, n, err) || !n || n.Id == "")
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        // Той самий санітар, що й у збереження: клієнт шле тіло сам. Id теж
        // приходить від клієнта й лягає в пейлоад чипа назавжди -- без
        // стелі підроблений carrier_add ніс би на чип мегабайтний Id (RPC
        // склеює частини без обмеження), який потім щоразу серіалізується у
        // ModStorage і їде назад кожному, хто натисне VIEW. Id -- це наш
        // "дата#seq", ~24 байти; 64 з запасом.
        n.Id    = OZ_Text.Clip(n.Id, 64);
        n.Title = OZ_Text.Clip(n.Title, OZ_PdaConst.NOTE_TITLE_MAX);
        n.Body  = OZ_Text.Clip(n.Body, OZ_PdaConst.NOTE_BODY_MAX);

        OZ_DataCarrier_Base c = OZ_CarrierOps.ResolveWritable(sender, "notes", error);
        if (!c)
            return "";

        OZ_NoteBook book = new OZ_NoteBook();
        if (c.OZ_IsWritten() && c.OZ_Payload() != "")
        {
            OZ_NoteBook parsed;
            if (JsonFileLoader<OZ_NoteBook>.LoadData(c.OZ_Payload(), parsed, err) && parsed && parsed.Notes)
                book = parsed;
        }

        int at = -1;
        for (int i = 0; i < book.Notes.Count(); i++)
        {
            if (book.Notes[i].Id == n.Id)
            {
                at = i;
                break;
            }
        }

        if (at >= 0)
        {
            book.Notes.Set(at, n);
        }
        else
        {
            OZ_CarrierSpec spec = OZ_PdaHardware.CarrierFor(c.GetType());
            if (spec && spec.MaxNotes > 0 && book.Notes.Count() >= spec.MaxNotes)
            {
                error = "STR_OZ_ERR_NOTES_FULL";
                return "";
            }
            book.Notes.Insert(n);
        }

        string outJson;
        if (!JsonFileLoader<OZ_NoteBook>.MakeData(book, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        c.OZ_Write("notes", outJson, book.Notes.Count());

        ok = true;
        error = "";
        return "";
    }

    private string List(PlayerIdentity sender, out string error)
    {
        string uid = sender.GetPlainId();

        OZ_NotesAskList a = new OZ_NotesAskList();
        a.Uid = uid;

        string letter;
        string err;
        if (!JsonFileLoader<OZ_NotesAskList>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("notes: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/notes/list", letter, new OZ_NotesReply(uid, "list", true));

        error = OZ_Const.DEFER;
        return "";
    }

    private string Save(string json, PlayerIdentity sender, out string error)
    {
        OZ_Note incoming;
        string err;
        if (!JsonFileLoader<OZ_Note>.LoadData(json, incoming, err) || !incoming)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        // Текст із клієнта чиститься ЗАВЖДИ: він поїде в JSON і в Discord,
        // обидва мають керівні символи, і жоден не має приймати чуже як є.
        incoming.Title = MiscGameplayFunctions.SanitizeString(incoming.Title);
        incoming.Body  = MiscGameplayFunctions.SanitizeString(incoming.Body);

        incoming.Title = OZ_Text.Clip(incoming.Title, OZ_PdaConst.NOTE_TITLE_MAX);
        incoming.Body = OZ_Text.Clip(incoming.Body, OZ_PdaConst.NOTE_BODY_MAX);

        string uid = sender.GetPlainId();

        OZ_NotesAskSave a = new OZ_NotesAskSave();
        a.Uid   = uid;
        a.Id    = incoming.Id;
        a.Title = incoming.Title;
        a.Body  = incoming.Body;
        a.Name  = sender.GetName();

        string letter;
        if (!JsonFileLoader<OZ_NotesAskSave>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("notes: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        // Відповідь моста -- { Id } -- їде клієнтові ЯК Є: саме її чекає
        // сторінка, щоб друге «Зберегти» правило записку, а не плодило дубль.
        OZ_BridgeClient.Call("v1/notes/save", letter, new OZ_NotesReply(uid, "save", true));

        error = OZ_Const.DEFER;
        return "";
    }

    private string Delete(string json, PlayerIdentity sender, out string error)
    {
        OZ_NoteRef r;
        string err;
        if (!JsonFileLoader<OZ_NoteRef>.LoadData(json, r, err) || !r)
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
            OZ_Log.Error("notes: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call("v1/notes/delete", letter, new OZ_NotesReply(uid, "delete", false));

        error = OZ_Const.DEFER;
        return "";
    }
}
