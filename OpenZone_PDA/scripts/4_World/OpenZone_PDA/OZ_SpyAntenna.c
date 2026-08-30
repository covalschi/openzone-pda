// Шпигунська антена: бачить УСІХ, у кого транспондер узагалі увімкнений,
// байдуже до рівня приватності -- але живе лічені хвилини активної роботи
// (SpyMinutes у Hardware.json), а вичерпавшись -- згорає, як дешифратор.
//
// Ресурс живе В ПЛАТІ, не в КПК: перекинув модуль -- ресурс поїхав з ним.

class OZ_Module_SpyAntenna : ItemBase
{
    // Секунди роботи, що лишились. -1 -- плата свіжа, ще не заряджалась
    // від спеки (стеля відома лише конфігові).
    private float m_SpyLeftS = -1;

    float OZ_SpyLeftS()
    {
        return m_SpyLeftS;
    }

    // Списати dt секунд. true -- ресурс щойно скінчився.
    bool OZ_SpyDrain(float dt, float defaultS)
    {
        if (!GetGame().IsServer())
            return false;

        if (m_SpyLeftS < 0)
            m_SpyLeftS = defaultS;

        if (m_SpyLeftS <= 0)
            return false;

        m_SpyLeftS = m_SpyLeftS - dt;
        return m_SpyLeftS <= 0;
    }

    bool OZ_SpyAlive()
    {
        // Свіжа (-1) або з рештою ресурсу; згоріла плата й так відпаде
        // через IsRuined в OZ_ModuleClass.
        return m_SpyLeftS < 0 || m_SpyLeftS > 0;
    }

    override void CF_OnStoreSave(CF_ModStorageMap storage)
    {
        super.CF_OnStoreSave(storage);

        auto ctx = storage["OpenZone_PDA"];
        if (!ctx)
            return;

        ctx.Write(m_SpyLeftS);
    }

    override bool CF_OnStoreLoad(CF_ModStorageMap storage)
    {
        if (!super.CF_OnStoreLoad(storage))
            return false;

        auto ctx = storage["OpenZone_PDA"];
        if (!ctx)
            return true;

        if (!ctx.Read(m_SpyLeftS))
            return false;

        return true;
    }
}

// Тікер антен: звичайній антені робити нічого, а ШПИГУНСЬКА щотіка
// списує ресурс -- поки КПК увімкнений і плата в гнізді.
class OZ_SpyAntennaBehaviour : OZ_ModuleBehaviour
{
    override string Kind()
    {
        return "antenna";
    }

    override string Owner()
    {
        return "OpenZone_PDA";
    }

    override float TickSeconds()
    {
        return 5;
    }

    override void OnTick(ItemBase pda, Man owner, float deltaSeconds)
    {
        OZ_PDA_Base dev = OZ_PDA_Base.Cast(pda);
        if (!dev)
            return;

        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
        {
            string cls = dev.OZ_ModuleClass(i);
            if (cls == "")
                continue;

            OZ_ModuleSpec spec = OZ_PdaHardware.ModuleFor(cls);
            if (!spec || spec.SpyMinutes <= 0)
                continue;

            OZ_Module_SpyAntenna plate = OZ_Module_SpyAntenna.Cast(dev.OZ_Attached(OZ_PdaConst.ModuleSlot(i)));
            if (!plate)
                continue;

            if (plate.OZ_SpyDrain(deltaSeconds, spec.SpyMinutes * 60))
            {
                // Ресурс вийшов -- плата згорає, слід чесний.
                plate.SetHealth("", "", 0);
                OZ_Log.Info("pda: spy antenna burnt out on " + dev.GetType());
            }
        }
    }
}
