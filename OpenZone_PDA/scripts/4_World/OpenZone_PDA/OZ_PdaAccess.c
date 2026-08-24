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
    override bool Check(PlayerIdentity who, string pageId, string op)
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

        // Пристрій міг замкнутись, поки лежав у рюкзаку. Рахуємо це тут,
        // ліниво -- будити тік заради кожного КПК на сервері марно.
        pda.OZ_EvaluateLock(prof.LockAfterMinutes);

        // ЗАМОК не стосується тих операцій, які для того й існують, щоб його
        // зняти. Небезпеки в цьому немає: і unlock, і setpin усе одно
        // вимагають ЗНАТИ код, і рахують невдалі спроби. Без цього винятку
        // код нема куди ввести -- гейт відкидає саме той запит, який мав би
        // відімкнути пристрій.
        if (!IsLockOp(op) && !pda.OZ_IsUnlocked())
            return false;

        // Друге питання коду для окремих сторінок -- поки що просто вимагає
        // відімкненого пристрою; окреме підтвердження приїде разом із UI.
        return true;
    }

    private bool IsLockOp(string op)
    {
        return op == "unlock" || op == "setpin";
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
    // Знайти КПК, який гравець тримає або несе.
    //
    // Спершу руки, потім інвентар: пристрій у руках -- це той, з яким гравець
    // працює зараз, і якщо їх два, брати треба саме його.
    static OZ_PDA_Base HeldBy(PlayerIdentity who)
    {
        PlayerBase player = OZ_PdaLookup.PlayerOf(who);
        if (!player)
            return null;

        OZ_PDA_Base inHands = OZ_PDA_Base.Cast(player.GetItemInHands());
        if (inHands)
            return inHands;

        array<EntityAI> items = new array<EntityAI>();
        player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);

        for (int i = 0; i < items.Count(); i++)
        {
            OZ_PDA_Base pda = OZ_PDA_Base.Cast(items[i]);
            if (pda)
                return pda;
        }

        return null;
    }

    static PlayerBase PlayerOf(PlayerIdentity who)
    {
        if (!who)
            return null;

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            PlayerBase p = PlayerBase.Cast(players[i]);
            if (p && p.GetIdentity() && p.GetIdentity().GetPlainId() == who.GetPlainId())
                return p;
        }

        return null;
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
