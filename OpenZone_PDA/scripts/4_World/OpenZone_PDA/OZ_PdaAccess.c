// Хто до якої сторінки допущений.
//
// Ядро цього вирішити не може: пристроїв воно не має. Тут -- нащадок його
// бази, який дивиться на КПК в руках гравця й звіряє три речі підряд:
//
//   1. сторінка входить у набір ПРОФІЛЮ цього пристрою;
//   2. якщо її вмикає модуль -- модуль справді вставлений;
//   3. якщо профіль позначив її як захищену піном -- пристрій відімкнений.
//
// Кожна перевірка серверна. Клієнт може попросити що завгодно; відповідає
// саме цей код, і його рішення остаточне.

class OZ_PdaAccess : OZ_PageAccess
{
    override bool Check(PlayerIdentity who, string pageId, string op, out string why)
    {
        if (!who)
            return false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(who);
        if (!pda)
        {
            // Пристрою немає -- лишається «віртуальний КПК», якщо його
            // увімкнув адмін.
            return OZ_PdaLookup.VirtualAllows(pageId);
        }

        OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());
        if (!prof)
            return false;

        if (prof.Pages.Find(pageId) == -1)
        {
            // Сторінки може не бути в профілі, але її може вмикати
            // вставлений модуль.
            if (!ModuleEnables(pda, pageId))
                return false;
        }

        // ВИМКНЕНИЙ ПРИСТРІЙ НЕ ПОКАЗУЄ НІЧОГО.
        //
        // Раніше живлення не питали тут узагалі -- ворота дивились на профіль,
        // модуль і замок, -- і сторінка контактів спокійно малювала список
        // Зони під написом POWERED DOWN у власному рядку стану. Вимкнений
        // прилад, який усе показує, -- це не прилад, а вікно.
        //
        // Відмовляє СЕРВЕР, а не малювальник: інакше дані все одно доїхали б
        // до клієнта, просто не потрапили б на екран, і вимкнути КПК було б
        // способом не бачити, а не способом не знати.
        //
        // Панель самого пристрою -- єдиний виняток, і він вимушений: саме на
        // ній кнопка «увімкнути». Без винятку прилад не можна було б увімкнути
        // ніколи.
        if (!pda.OZ_IsOn())
        {
            if (pageId != OZ_PdaConst.PAGE_DEVICE)
            {
                why = "STR_OZ_ERR_POWERED_DOWN";
                return false;
            }
        }

        // Пристрій міг замкнутись, поки лежав у рюкзаку. Рахуємо це тут,
        // ліниво -- будити тік заради кожного КПК на сервері марно.
        pda.OZ_EvaluateLock(prof.LockAfterMinutes);

        // ЗАМОК не стосується тих операцій, які для того й існують, щоб його
        // зняти. Небезпеки в цьому немає: і unlock, і setpin усе одно
        // вимагають ЗНАТИ код, і рахують невдалі спроби. Без цього винятку
        // код нема куди ввести -- гейт відкидає саме той запит, який мав би
        // відімкнути пристрій.
        if (!IsLockOp(op) && !pda.OZ_IsUnlocked())
        {
            why = "STR_OZ_ERR_LOCKED";
            return false;
        }

        // Друге питання коду для окремих сторінок -- поки що просто вимагає
        // відімкненого пристрою; окреме підтвердження приїде разом із UI.
        return true;
    }

    private bool IsLockOp(string op)
    {
        // crack -- теж операція про замок, і теж мусить проходити крізь
        // гейт: запечатаний пристрій НЕ віддає нічого, і без цього винятку
        // дешифратор не мав би куди підключитись.
        return op == "unlock" || op == "setpin" || op == "crack" || op == "sealed";
    }

    private bool ModuleEnables(OZ_PDA_Base pda, string pageId)
    {
        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
        {
            string cls = pda.OZ_ModuleClass(i);
            if (cls == "")
                continue;

            OZ_ModuleSpec spec = OZ_PdaHardware.ModuleFor(cls);
            if (!spec || !spec.EnablesPages)
                continue;

            if (spec.EnablesPages.Find(pageId) != -1)
                return true;
        }
        return false;
    }
}

class OZ_PdaLookup
{
    // Буфер під обхід інвентаря: один на цього ходока, а не new на кожен
    // виклик. EnumerateInventory оголошений як `out array<EntityAI> items`
    // (inventory.c:127) -- він ЗАПОВНЮЄ переданий масив, а не повертає свій,
    // тож алокація на кожен виклик була чистою витратою: на сорока гравцях
    // це порядку 2400 масивів за хвилину.
    //
    // ПРАВИЛО ДО БУФЕРА: не тримати його між викликами. Він спільний, і
    // вкладений виклик перезаповнить його під ногами того, хто ітерує.
    // Сьогодні вкладених немає -- перевірено по всіх дванадцяти місцях
    // виклику HeldBy. `ref` стоїть на МАСИВІ (він наш), а не на елементах:
    // сутності належать рушію, і ref на них тримав би їх живими після
    // знищення.
    private static ref array<EntityAI> s_Walk;

    // Знайти КПК, який гравець тримає або несе.
    //
    // Спершу руки, потім інвентар: пристрій у руках -- це той, з яким гравець
    // працює зараз, і якщо їх два, брати треба саме його.
    static OZ_PDA_Base HeldBy(PlayerIdentity who)
    {
        return HeldByPlayer(OZ_PdaLookup.PlayerOf(who));
    }

    // Для тих, хто вже ТРИМАЄ гравця. Прохід по онлайну, який на кожному
    // кроці добуває особу лише щоб тут-таки перетворити її назад на гравця,
    // -- це зайвий круг; OZR_Set.Sync робить саме так, для кожного гравця
    // кожні дві секунди. Рація припаркована, тож правка там окрема, але
    // точка, куди їй звертатись, є вже зараз.
    //
    // Живість тут НЕ перевіряється навмисне: труп із КПК у рюкзаку -- це
    // законне питання для того, хто його обшукує. Фільтрувати мертвих
    // мусить той, хто обходить онлайн, і кожен за своїм правилом.
    static OZ_PDA_Base HeldByPlayer(PlayerBase player)
    {
        if (!player)
            return null;

        OZ_PDA_Base inHands = OZ_PDA_Base.Cast(player.GetItemInHands());
        if (inHands)
            return inHands;

        // Ваниль сама перевіряє GetInventory() на сутностях у русі
        // (weapon_base.c:1163), і після цієї правки сюди заходить більше
        // викликів, ніж раніше.
        GameInventory inv = player.GetInventory();
        if (!inv)
            return null;

        if (!s_Walk)
            s_Walk = new array<EntityAI>();

        inv.EnumerateInventory(InventoryTraversalType.PREORDER, s_Walk);

        for (int i = 0; i < s_Walk.Count(); i++)
        {
            OZ_PDA_Base pda = OZ_PDA_Base.Cast(s_Walk[i]);
            if (pda)
                return pda;
        }

        return null;
    }

    static PlayerBase PlayerOf(PlayerIdentity who)
    {
        if (!who)
            return null;

        // Рушій сам знає, кому належить особа: `proto Man GetPlayer()` на
        // PlayerIdentityBase (gameplay.c:374). Один перехід.
        //
        // Тут стояв обхід GetPlayers() зі звіркою GetPlainId(). Кожна
        // ітерація коштувала переходів І ДВОХ АЛОКАЦІЙ РЯДКІВ -- GetPlainId
        // оголошений `proto string`, тобто щоразу породжує рядок, і права
        // частина порівняння стояла всередині циклу. На сорока гравцях це
        // близько сорока рядків сміття на виклик при понад трьох тисячах
        // викликів за хвилину. Не арифметика була дорога, а сміття: воно
        // приходить до збирача ривком, і ривок -- це і є лагспайк.
        //
        // ДОГОВІР МІНЯЄТЬСЯ В ОДНОМУ МІСЦІ, і це навмисне. За УСТАРІЛОЮ
        // особою обхід знаходив гравця, який ПЕРЕПІДКЛЮЧИВСЯ -- Steam-id той
        // самий, тож звірка збігалась, -- а GetPlayer() поверне null, бо
        // особа більше нікому не належить. Друге правильніше: діяти за
        // протухлою особою не можна. Але це зміна поведінки, а не чиста
        // оптимізація, і вона записана тут, а не лишена на виявлення.
        //
        // Перевірка `!who` лишається першим рядком: HeldBy -- публічний
        // static із дванадцятьма місцями виклику, а звернення до null у
        // Enforce обриває виконання посеред обробника, лишаючи вже зроблені
        // зміни застосованими наполовину.
        return PlayerBase.Cast(who.GetPlayer());
    }

    static bool VirtualAllows(string pageId)
    {
        OZ_PdaProfilesConfig cfg = OZ_PdaProfiles.Get();
        if (!cfg || !cfg.VirtualDevice || !cfg.VirtualDevice.Enabled)
            return false;
        if (!cfg.VirtualDevice.Pages)
            return false;
        return cfg.VirtualDevice.Pages.Find(pageId) != -1;
    }
}
