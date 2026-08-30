// Обмін контактами -- серверна половина.
//
// Одна дія на двох, і вона ж пропозиція, і вона ж згода. Перший тик лишає
// пропозицію, зустрічний -- замикає обмін. Окремого «прийняти» немає навмисно:
// воно означало б, що половина обміну відбувається в меню, а тоді перша
// половина -- та, що в світі, -- ні на що не впливає.
//
// Пропозиції живуть у файлі акаунта (FriendReq) -- там, де вони вже жили. Це
// не зайвий стан: без них зустрічний тик не мав би що замикати, а гравець,
// який тикнув першим і відійшов на крок, мусив би починати спочатку.
//
// СТРОК потрібен, бо в списку їх більше не видно. Пропозиція, яку ніхто не
// бачить і яка не тухне, -- це пастка: підійшов через годину, тикнув один раз
// і несподівано вже в контактах.

class OZ_PdaContactSwap
{
    // Скільки чекати зустрічного тику -- з Tuning.json: «поки ви стоїте
    // поруч», а не «поки ви обидва на сервері».

    // ПАРА «хто -> кому» -> коли пропозиція протухне.
    //
    // Ключем був сам отримувач, і це ламало строк рівно там, де він потрібен.
    // Пропозиція A для B протухала за хвилину -- аж поки повз не проходив C і
    // не тикав приладом у того ж B: його пропозиція клала НОВИЙ строк під той
    // самий ключ, і разом із нею оживала стара, забута пропозиція A. B тикав
    // у відповідь -- і опинявся в контактах у A, з яким розминувся годину тому.
    //
    // Пара -- те, чого строк насправді стосується. Список у файлі акаунта від
    // цього не міняється; тут лише час.
    private static ref map<string, int> s_Until;

    // from/to -- ЛЮДИ біля яких це відбувається (їм їдуть повідомлення);
    // myUid/theirUid -- АКАУНТИ, чиї пристрої потисли руки. Для власного
    // КПК це збігається; для чужого живого -- ні, і це навмисно: рішення
    // власника 2026-08-29, контакт належить сесії пристрою.
    static void Offer(PlayerIdentity from, PlayerIdentity to, string myUid, string theirUid)
    {
        if (!GetGame().IsServer())
            return;
        if (!from || !to)
            return;

        if (myUid == theirUid)
            return;

        OZ_PlayerData me   = OZ_PlayerStore.Load(myUid);
        OZ_PlayerData them = OZ_PlayerStore.Load(theirUid);

        if (Has(me.Friends, theirUid))
        {
            Say(from, "STR_OZ_SWAP_ALREADY");
            return;
        }

        // ЗУСТРІЧНИЙ ТИК: він уже пропонував мені -- замикаємо.
        if (Has(me.FriendReq, theirUid))
        {
            if (!Fresh(theirUid, myUid))
            {
                // Пропозиція протухла. Прибираємо й починаємо як з нуля --
                // цей самий тик стає новою пропозицією з мого боку.
                Drop(me.FriendReq, theirUid);
                OZ_PlayerStore.MarkDirty(myUid);
            }
            else
            {
                Drop(me.FriendReq, theirUid);
                Forget(theirUid, myUid);

                // Пишемо ОБОМ. Контакт взаємний, і однобокий запис зробив би
                // його видимим лише з одного боку -- тобто зламаним там, де
                // це найважче помітити.
                if (!Has(me.Friends, theirUid))
                    me.Friends.Insert(theirUid);
                if (!Has(them.Friends, myUid))
                    them.Friends.Insert(myUid);

                OZ_PlayerStore.MarkDirty(myUid);
                OZ_PlayerStore.MarkDirty(theirUid);

                // Руки потиснуто знову -- заморожена колись розмова
                // відмикається.
                OZ_PairFreeze.Send("v1/chat/pair_thaw", myUid, theirUid);

                Say(from, "STR_OZ_SWAP_DONE");
                Say(to,   "STR_OZ_SWAP_DONE");
                return;
            }
        }

        // ПРИБИРАЄМО ЗА СОБОЮ.
        //
        // FriendReq писався й ніколи не чистився: кожен тик приладом у
        // незнайомця лишав у ЙОГО файлі рядок, і той рядок не прибирало ніщо
        // -- ні строк, ні відмова. Через місяць гри у файлі активного гравця
        // лежали сотні протухлих пропозицій, які нічого не означають, але
        // їдуть із ним у кожне збереження.
        Prune(them);

        // ПЕРШИЙ ТИК: лишаємо пропозицію в нього.
        if (!Has(them.FriendReq, myUid))
        {
            them.FriendReq.Insert(myUid);
            OZ_PlayerStore.MarkDirty(theirUid);
        }

        Remember(myUid, theirUid);

        Say(from, "STR_OZ_SWAP_OFFERED");
        Say(to,   "STR_OZ_SWAP_ASKED");
    }

    // ------------------------------------------------------------- строк

    private static string Pair(string fromUid, string toUid)
    {
        return fromUid + ">" + toUid;
    }

    private static void Remember(string fromUid, string toUid)
    {
        if (!s_Until)
            s_Until = new map<string, int>();
        s_Until.Set(Pair(fromUid, toUid), GetGame().GetTime() + OZ_PdaTune.SwapOfferTtlMs());
    }

    private static void Forget(string fromUid, string toUid)
    {
        string k = Pair(fromUid, toUid);
        if (s_Until && s_Until.Contains(k))
            s_Until.Remove(k);
    }

    // Строку немає -- пропозиція пережила перезапуск сервера. Вважаємо
    // протухлою: тримати в силі те, про що ми не пам'ятаємо, коли воно було
    // зроблене, -- це і є пастка, від якої строк узагалі заведено.
    private static bool Fresh(string fromUid, string toUid)
    {
        if (!s_Until)
            return false;

        int until;
        if (!s_Until.Find(Pair(fromUid, toUid), until))
            return false;

        return GetGame().GetTime() < until;
    }

    // Викинути з його списку все, чий строк вийшов. Дешево: список короткий,
    // а виклик трапляється лише коли хтось справді тикнув приладом.
    private static void Prune(OZ_PlayerData them)
    {
        if (!them || !them.FriendReq)
            return;

        bool touched = false;

        for (int i = them.FriendReq.Count() - 1; i >= 0; i--)
        {
            if (Fresh(them.FriendReq[i], them.SteamId))
                continue;

            Forget(them.FriendReq[i], them.SteamId);
            them.FriendReq.Remove(i);
            touched = true;
        }

        if (touched)
            OZ_PlayerStore.MarkDirty(them.SteamId);
    }

    // ------------------------------------------------------------ дрібне

    private static bool Has(array<string> list, string uid)
    {
        if (!list)
            return false;
        return list.Find(uid) != -1;
    }

    private static void Drop(array<string> list, string uid)
    {
        if (!list)
            return;

        int at = list.Find(uid);
        if (at != -1)
            list.Remove(at);
    }

    // Кажемо ОБОМ і в тому ж каналі, що й решта відповідей КПК: обмін --
    // подія в світі, і мовчазна дія лишила б обох гадати, спрацювало чи ні.
    static void Say(PlayerIdentity who, string key)
    {
        if (!who)
            return;
        OZ_Rpc.RoleRespond(who, "swap", true, key);
    }
}
