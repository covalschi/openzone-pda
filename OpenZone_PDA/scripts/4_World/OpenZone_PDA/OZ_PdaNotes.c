// Сторінка «Записки»: нотатки, які живуть У ПРИСТРОЇ.
//
// ДЖЕРЕЛО ПРАВДИ -- САМ ПРИЛАД. Рішення власника 2026-08-28: записки
// відв'язані від Discord і від акаунта разом -- це пам'ять пристрою, як
// мітки на карті. Наслідки всі навмисні:
//
//   -- захоплений КПК віддає записки попереднього носія: це здобич;
//   -- загублений прилад забирає їх із собою, смерть -- теж;
//   -- капсула читає свою книжку без жодного зрізу: після заморозки в
//      неї ніхто нічого не допише, тож книжка і Є зріз;
//   -- моста не треба взагалі: всі операції синхронні, і записки
//      працюють навіть тоді, коли Discord лежить.
//
// Стеля книжки -- справа моделі пристрою (Limits.Notes у профілі),
// запасна -- OZ_PdaConst.NOTES_MAX.


// Книжка цілком -- формат відповіді list. Той самий клас читала стара
// файлова версія, тож клієнт розбирає обидві епохи одним описом.
class OZ_NoteBook
{
    int Version = 1;
    // Стеля книжки, як її знає міст. 0 у старих знімках на чипах -- тоді
    // читач бере власну OZ_PdaConst.NOTES_MAX як запасну.
    int Max = 0;
    // Читальня капсули: книжка зрізана по заморозці, писати нема куди.
    bool Frozen = false;
    ref array<ref OZ_Note> Notes;

    void OZ_NoteBook()
    {
        Notes = new array<ref OZ_Note>();
    }
}

// -------------------------------------------------------------- сторінка

class OZ_PdaHandlerNotes : OZ_PageHandler
{
    // Той самий прийом, що в міток: час у секундах зіткнувся б на двох
    // записках в одну секунду, тому до нього додається лічильник.
    private static int s_NoteSeq = 0;

    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok    = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        // Експорт на носій: тіло записки клієнт має з list і шле сам, чип
        // лежить у слоті. Пристрій цій операції не потрібен.
        if (op == "carrier_add")
            return CarrierAdd(json, sender, ok, error);

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        // КАПСУЛА читається, але не пишеться. Зрізати книжку нема по чому
        // й не треба: після заморозки в неї ніхто нічого не допише, тож
        // вона і Є стан на мить заморозки.
        bool frozen = OZ_PdaCapsule.IsFrozen(pda);
        if (frozen && op != "list")
        {
            error = "STR_OZ_ERR_FROZEN";
            return "";
        }

        if (op == "list")
            return List(pda, frozen, ok, error);

        if (op == "save")
            return Save(json, pda, ok, error);

        if (op == "delete")
            return Delete(json, pda, ok, error);

        return "";
    }

    // Книжка пристрою. Нечитна -- починаємо чисту, але слід у лозі
    // лишається: мовчки згубити чиїсь записки не можна.
    private OZ_NoteBook BookOf(OZ_PDA_Base pda)
    {
        OZ_NoteBook book = new OZ_NoteBook();
        if (pda.OZ_NotesJson() == "")
            return book;

        string err;
        OZ_NoteBook parsed;
        if (JsonFileLoader<OZ_NoteBook>.LoadData(pda.OZ_NotesJson(), parsed, err) && parsed && parsed.Notes)
            return parsed;

        OZ_Log.Warn("notes: unreadable book on " + pda.GetType() + ", starting fresh (" + err + ")");
        return book;
    }

    private int LimitOf(OZ_PDA_Base pda)
    {
        OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());
        if (prof && prof.Limits && prof.Limits.Notes > 0)
            return prof.Limits.Notes;
        return OZ_PdaTune.NotesMax();
    }

    private bool Flush(OZ_PDA_Base pda, OZ_NoteBook book, out string error)
    {
        string outJson;
        string err;
        if (!JsonFileLoader<OZ_NoteBook>.MakeData(book, outJson, err, false))
        {
            OZ_Log.Error("notes: cannot serialise the book: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return false;
        }

        pda.OZ_SetNotesJson(outJson);
        return true;
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
        n.Title = OZ_Text.Clip(n.Title, OZ_PdaTune.NoteTitleMax());
        n.Body  = OZ_Text.Clip(n.Body, OZ_PdaTune.NoteBodyMax());

        OZ_DataCarrier_Base c = OZ_CarrierOps.ResolveWritable(sender, error);
        if (!c)
            return "";

        OZ_NoteBook book = new OZ_NoteBook();
        if (c.OZ_Notes() != "")
        {
            OZ_NoteBook parsed;
            if (JsonFileLoader<OZ_NoteBook>.LoadData(c.OZ_Notes(), parsed, err) && parsed && parsed.Notes)
                book = parsed;
            else
                // Нечитна секція -- перезаписуємо свіжою книжкою: рятувати
                // там нема чого. Але МОВЧКИ губити чужі дані не можна --
                // слід у лозі каже, що на чипі щось БУЛО.
                OZ_Log.Warn("carrier: unreadable notes payload on " + c.GetType() + ", replacing (" + err + ")");
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
            // Місце -- носія, а не записника: тут заповнився ЧИП, і сказати
            // «записник повний» означало б показати не ту причину.
            int room = c.OZ_RoomFor(OZ_DataCarrier_Base.KIND_NOTES);
            if (room >= 0 && book.Notes.Count() >= room)
            {
                error = "STR_OZ_ERR_CARRIER_FULL";
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

        c.OZ_WriteNotes(outJson, book.Notes.Count());

        ok = true;
        error = "";
        return "";
    }

    private string List(OZ_PDA_Base pda, bool frozen, out bool ok, out string error)
    {
        OZ_NoteBook book = BookOf(pda);
        book.Max    = LimitOf(pda);
        book.Frozen = frozen;

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_NoteBook>.MakeData(book, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string Save(string json, OZ_PDA_Base pda, out bool ok, out string error)
    {
        OZ_Note incoming;
        string err;
        if (!JsonFileLoader<OZ_Note>.LoadData(json, incoming, err) || !incoming)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        // БЕЗ ванільного SanitizeString: він -- сліпий Substring(0,512) по
        // БАЙТАХ (miscgameplayfunctions.c:863), тобто і стелю NOTE_BODY_MAX
        // ламає, і кирилицю ріже навпіл. Наш Clip ріже по межі символу й по
        // НАШІЙ стелі; JSON сам екранує те, що йому треба.
        incoming.Title = OZ_Text.Clip(incoming.Title, OZ_PdaTune.NoteTitleMax());
        incoming.Body  = OZ_Text.Clip(incoming.Body, OZ_PdaTune.NoteBodyMax());

        OZ_NoteBook book = BookOf(pda);

        int at = -1;
        if (incoming.Id != "")
        {
            for (int i = 0; i < book.Notes.Count(); i++)
            {
                if (book.Notes[i].Id == incoming.Id)
                {
                    at = i;
                    break;
                }
            }

            // Id, якого в книжці немає, -- не «створи під ним»: клієнт
            // просив правку записки, якої більше не існує.
            if (at < 0)
            {
                error = "STR_OZ_ERR_NO_NOTE";
                return "";
            }
        }

        string now = OZ_Time.NowUtc();
        if (at >= 0)
        {
            book.Notes[at].Title    = incoming.Title;
            book.Notes[at].Body     = incoming.Body;
            book.Notes[at].EditedAt = now;
        }
        else
        {
            if (book.Notes.Count() >= LimitOf(pda))
            {
                error = "STR_OZ_ERR_NOTES_FULL";
                return "";
            }

            OZ_Note fresh = new OZ_Note();
            s_NoteSeq++;
            fresh.Id        = now + "#n" + s_NoteSeq.ToString();
            fresh.Title     = incoming.Title;
            fresh.Body      = incoming.Body;
            fresh.CreatedAt = now;
            fresh.EditedAt  = now;
            book.Notes.Insert(fresh);
            at = book.Notes.Count() - 1;
        }

        if (!Flush(pda, book, error))
            return "";

        // Відповідь -- { Id }: саме її чекає сторінка, щоб друге
        // «Зберегти» правило записку, а не плодило дубль.
        OZ_NoteRef saved = new OZ_NoteRef();
        saved.Id = book.Notes[at].Id;

        string outJson;
        if (!JsonFileLoader<OZ_NoteRef>.MakeData(saved, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string Delete(string json, OZ_PDA_Base pda, out bool ok, out string error)
    {
        OZ_NoteRef r;
        string err;
        if (!JsonFileLoader<OZ_NoteRef>.LoadData(json, r, err) || !r || r.Id == "")
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_NoteBook book = BookOf(pda);

        int at = -1;
        for (int i = 0; i < book.Notes.Count(); i++)
        {
            if (book.Notes[i].Id == r.Id)
            {
                at = i;
                break;
            }
        }

        if (at < 0)
        {
            error = "STR_OZ_ERR_NO_NOTE";
            return "";
        }

        book.Notes.RemoveOrdered(at);

        if (!Flush(pda, book, error))
            return "";

        ok = true;
        error = "";
        return "";
    }
}
