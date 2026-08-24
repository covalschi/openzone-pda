// Договір журналу завдань.
//
// КПК везе САМУ СТОРІНКУ й форму даних; квестовий мод везе тільки дані.
//
// Чому не «хай кожен реєструє свою сторінку»: тоді журналів стало б стільки ж,
// скільки квестових модів, кожен зі своїм виглядом, а КПК перетворився б на
// смітник вкладок. Спільний договір дає один журнал незалежно від того, чий
// мод його наповнює -- Expansion Quests, TerjeQuests, Rejects чи самописний.
//
// Постачальник підключається так: успадкувати OZ_QuestProvider, перевизначити
// Collect() і покласти себе через OZ_PdaQuests.Bind() зі свого OnMissionStart.
// Постачальник рівно один: два журнали в одному пристрої -- це знову дві
// правди. Другий Bind перезаписує перший і каже про це в лог.

class OZ_QuestObjective
{
    string Text     = "";
    bool   Done     = false;
    // Необов'язковий лічильник: «зібрано 3 з 5». Обидва нулі -- лічильника
    // немає й малювати його не треба.
    int    Current  = 0;
    int    Total    = 0;
}

class OZ_QuestEntry
{
    string Id       = "";
    string Title    = "";
    string Summary  = "";
    // "active" | "done" | "failed". Рядком, а не числом: журнал показує це
    // гравцеві, а мод-постачальник не мусить знати наших констант.
    string State    = "active";
    string Giver    = "";
    // Порожній рядок означає «місце невідоме» -- на карті нічого не малюємо.
    // Формат той самий, що в грі: "x y z".
    string Position = "";
    ref array<ref OZ_QuestObjective> Objectives;

    void OZ_QuestEntry()
    {
        Objectives = new array<ref OZ_QuestObjective>();
    }
}

class OZ_QuestJournal
{
    // Чи є взагалі постачальник. Порожній журнал і відсутній журнал -- різні
    // повідомлення для гравця, і плутати їх не можна.
    bool HasProvider = false;
    string ProviderName = "";
    ref array<ref OZ_QuestEntry> Entries;

    void OZ_QuestJournal()
    {
        Entries = new array<ref OZ_QuestEntry>();
    }
}

class OZ_QuestProvider
{
    // Ім'я мода-постачальника. Показується в журналі, щоб гравець розумів,
    // звідки взялись завдання, коли їх кілька джерел на сервері.
    string Name()
    {
        return "unknown";
    }

    // Кличеться серверно, на запит сторінки. Заповнити journal.Entries.
    void Collect(PlayerIdentity who, OZ_QuestJournal journal)
    {
    }
}

class OZ_PdaQuests
{
    private static ref OZ_QuestProvider s_Provider;

    static void Bind(OZ_QuestProvider provider)
    {
        if (s_Provider)
        {
            string w = "quest provider replaced: " + s_Provider.Name();
            w += " -> " + provider.Name();
            OZ_Log.Warn(w);
        }
        s_Provider = provider;
        OZ_Log.Info("quest provider: " + provider.Name());
    }

    static bool HasProvider()
    {
        return s_Provider != null;
    }

    static string ProviderName()
    {
        if (!s_Provider)
            return "";
        return s_Provider.Name();
    }

    static OZ_QuestJournal Collect(PlayerIdentity who)
    {
        OZ_QuestJournal j = new OZ_QuestJournal();

        if (!s_Provider)
            return j;   // HasProvider лишається false -- сторінка так і скаже

        j.HasProvider  = true;
        j.ProviderName = s_Provider.Name();
        s_Provider.Collect(who, j);
        return j;
    }
}
