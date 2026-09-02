// Замок гарячих клавіш VPP над вікнами КПК.
//
// Причина й механіка -- в OZ_VppKeys (OpenZone_VPP). Тут лише два вікна, які
// забирають у гравця керування, і тому мусять забрати й прив'язки адміна.
//
// Саме тут це й ловилося: на сторінці рації в КПК є поле імені частоти, і
// Backspace у ньому кидав у вільну камеру замість стерти літеру.
//
// КПК про VPP не знає й не мусить: цей файл лежить у pbo, який без VPP не
// вантажиться взагалі.

#ifdef AVPPAdminTools
#ifndef NO_GUI

modded class OZ_PdaMenu
{
    // Порядок навмисний -- див. OZ_VppKeys: super.OnShow() уміє закрити себе
    // ж, якщо layout не завантажився.
    override void OnShow()
    {
        OZ_VppKeys.Hold();
        super.OnShow();
    }

    override void OnHide()
    {
        super.OnHide();
        OZ_VppKeys.Release();
    }
}

// Редактор HUD: перетягування іконок мишею. Тексту тут немає, але Delete у
// VPP зносить об'єкт у прицілі, а H телепортує -- над вікном, де гравець
// знерухомлений, спрацювати не має нічого.
modded class OZ_HudEditMenu
{
    override void OnShow()
    {
        OZ_VppKeys.Hold();
        super.OnShow();
    }

    override void OnHide()
    {
        super.OnHide();
        OZ_VppKeys.Release();
    }
}

#endif
#endif
