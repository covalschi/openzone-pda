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
//
// СТРОК ЛЕЖИТЬ ПОРУЧ ІЗ ПРОПОЗИЦІЄЮ, у тому ж файлі (OZ_FriendReq.Until).
// Раніше він жив у мапі в пам'яті сервера, і це ламало його двічі. Рестарт
// стирав мапу, а список у файлі лишався -- і кожна збережена пропозиція
// ставала «протухлою назавжди», тобто зустрічний тик після рестарту не
// замикав обмін НІКОЛИ. А до того ключем була не пара, а сам отримувач, і
// чужа пропозиція оживляла давно забуту.

class OZ_PdaContactSwap
{
    // from/to -- ЛЮДИ біля яких це відбувається (їм їдуть повідомлення);
    // myUid/theirUid -- АКАУНТИ, чиї пристрої потисли руки. Для власного
    // КПК це збігається; для чужого живого -- ні, і це навмисно: рішення
    // власника 2026-08-29, контакт належить сесії пристрою.
    static void Offer(PlayerIdentity from, PlayerIdentity to, string myUid, string theirUid)
    {
        if (!GetGame().IsServer())
            return;

        // МОВЧАЗНИХ ВИХОДІВ ТУТ БІЛЬШЕ НЕМАЄ.
        //
        // Обидва ці випадки просто поверталися: гравець тикав приладом,
        // нічого не відбувалось, і жодного слова ні на екрані, ні в лозі.
        // Решта відмов у цій же функції говорить -- ці мовчали.
        if (!from || !to)
        {
            OZ_Log.Warn("swap: one of the two identities is gone, the exchange is dropped");
            Say(from, "STR_OZ_ERR_INTERNAL");
            return;
        }

        if (myUid == theirUid)
        {
            // Один акаунт по обидва боки: два свої прилади в руках, або чужий
            // живий прилад, сесію якого відкрив я ж сам.
            OZ_Log.Dbg("swap: both devices belong to " + myUid + ", nothing to exchange");
            Say(from, "STR_OZ_SWAP_SELF");
            return;
        }

        OZ_PlayerData me   = OZ_PlayerStore.Load(myUid);
        OZ_PlayerData them = OZ_PlayerStore.Load(theirUid);

        // У записнику люди позначені КЛЮЧЕМ ПЕРСОНАЖА, а не Steam64: після
        // пермадесу той самий акаунт -- уже інша людина, і знайомитись із
        // нею доводиться наново. Саме тому обмін пише ключі, а не uid-и.
        string myKey    = OZ_PlayerStore.KeyOf(myUid);
        string theirKey = OZ_PlayerStore.KeyOf(theirUid);

        if (Has(me.Friends, theirKey))
        {
            Say(from, "STR_OZ_SWAP_ALREADY");
            return;
        }

        // ЗУСТРІЧНИЙ ТИК: він уже пропонував мені -- замикаємо.
        int at = IndexOfReq(me.FriendReq, theirKey);
        if (at != -1)
        {
            bool fresh = Fresh(me.FriendReq[at]);
            me.FriendReq.Remove(at);
            OZ_PlayerStore.MarkDirty(myUid);

            // Протухла -- починаємо як з нуля: цей самий тик стає новою
            // пропозицією з мого боку, тобто провалюємось нижче.
            if (fresh)
            {
                // Пишемо ОБОМ. Контакт взаємний, і однобокий запис зробив би
                // його видимим лише з одного боку -- тобто зламаним там, де
                // це найважче помітити.
                if (!Has(me.Friends, theirKey))
                    me.Friends.Insert(theirKey);
                if (!Has(them.Friends, myKey))
                    them.Friends.Insert(myKey);

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

        // ПЕРШИЙ ТИК: лишаємо пропозицію в нього. Повторний тик у ту саму
        // людину ОНОВЛЮЄ строк -- він знову стоїть поруч, знову тикає.
        int mine = IndexOfReq(them.FriendReq, myKey);
        if (mine == -1)
        {
            OZ_FriendReq req = new OZ_FriendReq();
            req.Key   = myKey;
            req.Until = OZ_Time.InUtc(OZ_PdaTune.SwapOfferTtlMs() / 1000);
            them.FriendReq.Insert(req);
        }
        else
        {
            them.FriendReq[mine].Until = OZ_Time.InUtc(OZ_PdaTune.SwapOfferTtlMs() / 1000);
        }
        OZ_PlayerStore.MarkDirty(theirUid);

        Say(from, "STR_OZ_SWAP_OFFERED");
        Say(to,   "STR_OZ_SWAP_ASKED");
    }

    // ------------------------------------------------------------- строк

    static int IndexOfReq(array<ref OZ_FriendReq> list, string key)
    {
        if (!list)
            return -1;

        for (int i = 0; i < list.Count(); i++)
        {
            if (list[i] && list[i].Key == key)
                return i;
        }
        return -1;
    }

    // Порожній строк -- пропозиція старого зразка, з часів, коли строк жив у
    // пам'яті сервера. Вважаємо протухлою: тримати в силі те, про що ми не
    // пам'ятаємо, коли воно було зроблене, -- це і є пастка, від якої строк
    // узагалі заведено.
    private static bool Fresh(OZ_FriendReq req)
    {
        if (!req || req.Until == "")
            return false;

        return OZ_Time.Before(OZ_Time.NowUtc(), req.Until);
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
            if (Fresh(them.FriendReq[i]))
                continue;

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

    // Кажемо ОБОМ і в тому ж каналі, що й решта відповідей КПК: обмін --
    // подія в світі, і мовчазна дія лишила б обох гадати, спрацювало чи ні.
    static void Say(PlayerIdentity who, string key)
    {
        if (!who)
            return;
        OZ_Rpc.RoleRespond(who, "swap", true, key);
    }
}
