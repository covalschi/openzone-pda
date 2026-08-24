// Клієнтська точка входу КПК.
//
// У #0 меню ще немає, тож шлюз тримає заглушку: дія «відкрити» доходить і
// повідомляє про себе, а справжнє вікно приїде в Task 9 і замінить лише цей
// один клас. Решта -- дія, предмет, доступ -- уже готова й нічого не знає
// про UI.

class OZ_PdaMenuOpenerStub : OZ_PdaMenuGate
{
    override void DoOpen()
    {
        OZ_Log.Info("pda open requested (menu arrives in Task 9)");
    }

    override void DoClose()
    {
    }
}

modded class MissionGameplay
{
    override void OnInit()
    {
        super.OnInit();
        OZ_PdaMenuGate.Bind(new OZ_PdaMenuOpenerStub());
    }
}
