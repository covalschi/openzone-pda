// Сторінка «Записки»: приватні нотатки гравця.
//
// СЕРВЕРНІ, один файл на Steam64 у $profile:OpenZone\notes\. Не CF_ModStorage
// і не сам пристрій: записки належать АКАУНТУ, а не тілу й не залізу. Загинув
// -- записки лишились; загубив КПК -- лишились; знайшов чужий КПК -- своїх
// записок там немає, бо їх там ніколи й не було.
//
// Це ж і відповідь на «а якщо КПК украли»: красти нема чого. Записки
// прив'язані до того, хто дивиться, а не до того, що в руках.
//
// Дзеркало в тред Discord приїде разом із мостом і нічого тут не змінить:
// файл лишається джерелом правди, тред -- копією для читання з телефона.

class OZ_NoteBook : OZ_ConfigBase
{
    ref array<ref OZ_Note> Notes;

    override int LatestVersion()
    {
        return 1;
    }

    override void LoadDefaults()
    {
        Version = LatestVersion();
        Notes   = new array<ref OZ_Note>();
    }

    override void Validate(out int warnings)
    {
        warnings = 0;
        if (!Notes)
            Notes = new array<ref OZ_Note>();
    }
}

class OZ_NoteStore
{
    // Каталог НАШ, а не ядра: ядро не знає ні про які записки, і додавати
    // туди константу заради одного мода означало б, що наступний мод додасть
    // ще одну.
    static const string DIR = OZ_Const.PROFILE_DIR + "\\notes";

    private static string PathOf(string uid)
    {
        return DIR + "\\" + uid + ".json";
    }

    // MakeDirectory не рекурсивний, але $profile:OpenZone ядро вже створило
    // до нас: його ServerLoad іде першим.
    static void EnsureDir()
    {
        OZ_Json.EnsureDir(DIR);
    }

    static OZ_NoteBook Load(string uid)
    {
        OZ_NoteBook b = new OZ_NoteBook();
        // backup=false з тієї ж причини, що й у сховищі гравців: файлів
        // сотні, і копія кожного перед кожним записом зробила б Backup
        // непридатним для пошуку.
        OZ_ConfigLoader<OZ_NoteBook>.Load(PathOf(uid), "notes_" + uid, b, false);
        return b;
    }

    static void Save(string uid, OZ_NoteBook b)
    {
        OZ_ConfigLoader<OZ_NoteBook>.Save(PathOf(uid), "notes_" + uid, b, false);
    }
}

class OZ_PdaHandlerNotes : OZ_PageHandler
{
    // Ідентифікатор нової записки. Час у секундах був би зіткненням при двох
    // записках в одну секунду, тому до нього додається лічильник.
    private static int s_Seq = 0;

    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "list")
            return List(sender, ok, error);

        if (op == "save")
            return Save(json, sender, ok, error);

        if (op == "delete")
            return Delete(json, sender, ok, error);

        return "";
    }

    private string List(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_NoteBook b = OZ_NoteStore.Load(sender.GetPlainId());

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_NoteBook>.MakeData(b, outJson, err, false))
        {
            OZ_Log.Error("notes serialise failed: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string Save(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_Note incoming;
        string err;
        if (!JsonFileLoader<OZ_Note>.LoadData(json, incoming, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        // Текст із клієнта чиститься ЗАВЖДИ. Він поїде в JSON, а згодом у
        // тред Discord -- обидва мають свої керівні символи, і жоден не має
        // приймати те, що набрав хтось інший, як є.
        incoming.Title = MiscGameplayFunctions.SanitizeString(incoming.Title);
        incoming.Body  = MiscGameplayFunctions.SanitizeString(incoming.Body);

        if (incoming.Title.Length() > OZ_PdaConst.NOTE_TITLE_MAX)
            incoming.Title = incoming.Title.Substring(0, OZ_PdaConst.NOTE_TITLE_MAX);
        if (incoming.Body.Length() > OZ_PdaConst.NOTE_BODY_MAX)
            incoming.Body = incoming.Body.Substring(0, OZ_PdaConst.NOTE_BODY_MAX);

        string uid = sender.GetPlainId();
        OZ_NoteBook b = OZ_NoteStore.Load(uid);

        if (incoming.Id == "")
        {
            if (b.Notes.Count() >= OZ_PdaConst.NOTES_MAX)
            {
                error = "STR_OZ_ERR_NOTES_FULL";
                return "";
            }

            s_Seq++;
            incoming.Id = OZ_Time.NowUtc();
            incoming.Id += "#" + s_Seq.ToString();
            incoming.CreatedAt = OZ_Time.NowUtc();
            incoming.EditedAt  = incoming.CreatedAt;
            b.Notes.Insert(incoming);
        }
        else
        {
            int at = IndexOf(b, incoming.Id);
            if (at == -1)
            {
                // Записки з таким id немає. Це не привід створити нову: клієнт
                // просив ЗМІНИТИ щось конкретне, і мовчазна підміна дії -- це
                // те, що потім не знайдеш.
                error = "STR_OZ_ERR_NO_NOTE";
                return "";
            }

            b.Notes[at].Title    = incoming.Title;
            b.Notes[at].Body     = incoming.Body;
            b.Notes[at].EditedAt = OZ_Time.NowUtc();
        }

        OZ_NoteStore.Save(uid, b);

        ok = true;
        error = "";
        return "";
    }

    private string Delete(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_NoteRef r;
        string err;
        if (!JsonFileLoader<OZ_NoteRef>.LoadData(json, r, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string uid = sender.GetPlainId();
        OZ_NoteBook b = OZ_NoteStore.Load(uid);

        int at = IndexOf(b, r.Id);
        if (at == -1)
        {
            error = "STR_OZ_ERR_NO_NOTE";
            return "";
        }

        b.Notes.Remove(at);
        OZ_NoteStore.Save(uid, b);

        ok = true;
        error = "";
        return "";
    }

    private int IndexOf(OZ_NoteBook b, string id)
    {
        for (int i = 0; i < b.Notes.Count(); i++)
        {
            if (b.Notes[i].Id == id)
                return i;
        }
        return -1;
    }
}
