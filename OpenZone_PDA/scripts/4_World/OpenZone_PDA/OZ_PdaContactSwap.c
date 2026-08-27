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
    // Скільки чекати зустрічного тику. Хвилина -- це «поки ви стоїте поруч»,
    // а не «поки ви обидва на сервері».
    private static const int TTL_MS = 60000;

    // uid, кому запропонували -> коли пропозиція протухне.
    // Пара тримається в файлі акаунта; тут лише строк.
    private static ref map<string, int> s_Until;

    static void Offer(PlayerIdentity from, PlayerIdentity to)
    {
        if (!GetGame().IsServer())
            return;
        if (!from || !to)
            return;

        string myUid    = from.GetPlainId();
        string theirUid = to.GetPlainId();

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
            if (!Fresh(myUid))
            {
                // Пропозиція протухла. Прибираємо й починаємо як з нуля --
                // цей самий тик стає новою пропозицією з мого боку.
                Drop(me.FriendReq, theirUid);
                OZ_PlayerStore.MarkDirty(myUid);
            }
            else
            {
                Drop(me.FriendReq, theirUid);
                Forget(myUid);

                // Пишемо ОБОМ. Контакт взаємний, і однобокий запис зробив би
                // його видимим лише з одного боку -- тобто зламаним там, де
                // це найважче помітити.
                if (!Has(me.Friends, theirUid))
                    me.Friends.Insert(theirUid);
                if (!Has(them.Friends, myUid))
                    them.Friends.Insert(myUid);

                OZ_PlayerStore.MarkDirty(myUid);
                OZ_PlayerStore.MarkDirty(theirUid);

                Say(from, "STR_OZ_SWAP_DONE");
                Say(to,   "STR_OZ_SWAP_DONE");
                return;
            }
        }

        // ПЕРШИЙ ТИК: лишаємо пропозицію в нього.
        if (!Has(them.FriendReq, myUid))
        {
            them.FriendReq.Insert(myUid);
            OZ_PlayerStore.MarkDirty(theirUid);
        }

        Remember(theirUid);

        Say(from, "STR_OZ_SWAP_OFFERED");
        Say(to,   "STR_OZ_SWAP_ASKED");
    }

    // ------------------------------------------------------------- строк

    private static void Remember(string uid)
    {
        if (!s_Until)
            s_Until = new map<string, int>();
        s_Until.Set(uid, GetGame().GetTime() + TTL_MS);
    }

    private static void Forget(string uid)
    {
        if (s_Until && s_Until.Contains(uid))
            s_Until.Remove(uid);
    }

    // Строку немає -- пропозиція пережила перезапуск сервера. Вважаємо
    // протухлою: тримати в силі те, про що ми не пам'ятаємо, коли воно було
    // зроблене, -- це і є пастка, від якої строк узагалі заведено.
    private static bool Fresh(string uid)
    {
        if (!s_Until)
            return false;

        int until;
        if (!s_Until.Find(uid, until))
            return false;

        return GetGame().GetTime() < until;
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
    private static void Say(PlayerIdentity who, string key)
    {
        if (!who)
            return;
        OZ_Rpc.RoleRespond(who, "swap", true, key);
    }
}
