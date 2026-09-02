// Меню КПК: корпус пристрою поверх приглушеного світу.
//
// Стрічка вкладок будується не з реєстру й не з конфіга, а з ВІДПОВІДІ
// СЕРВЕРА про той пристрій, що в руках. Причина: профілі серверні, і
// вирішувати, які вкладки в тебе є, не має права клієнт. Тому меню
// відкривається порожнім, надсилає device/status і домальовує себе.
//
// Один запит, а не два: сторінка «Пристрій» усе одно питає той самий status,
// і її ж відповідь будує стрічку.

class OZ_PdaMenu : UIScriptedMenu
{
    private Widget m_TabRail;
    private Widget m_PageHost;
    private Widget m_LockPanel;
    private Widget m_InitPanel;
    private ButtonWidget m_BtnClose;

    private ref map<string, ref OZ_PdaPage> m_Pages;
    private ref array<Widget> m_Tabs;
    private string m_Current = "";
    private bool m_Built = false;

    // Чи був увімкнений ванільний інтерфейс до відкриття КПК. Повертаємо
    // САМЕ ЦЕ, а не «увімкнено» -- див. OnShow/OnHide.
    private bool m_HudWasShown = true;

    private ref Timer m_Refresh;

    // Введений код накопичується тут і НІКУДИ більше: на сервер їде рівно
    // один раз, коли гравець підтвердив. Порівнює його сервер.
    private string m_PinBuffer = "";

    // Екран коду має ТРИ приводи з'явитись, і плутати їх не можна:
    //
    //   ""        нікому не треба, панель схована;
    //   "unlock"  пристрій замкнений -- панель ПРИМУСОВА, скасувати не можна;
    //   "set"     гравець сам попросив задати або змінити код;
    //   "clear"   гравець сам попросив зняти код.
    //
    // Різниця не косметична: примусову панель не можна закрити по Esc, а
    // добровільну -- треба, інакше вийти з неї нема як.
    private string m_PinMode = "";

    // Що сказала ОСТАННЯ відповідь `sealed`. Єдиний читач -- RefreshTick,
    // щоб не просити status у пристрою, який його не віддасть. Хибне
    // значення тут не ламає нічого, а лише повертає зайвий запит на секунду:
    // рішення про доступ ухвалює сервер, і цей прапорець його не стосується.
    private bool m_Sealed = false;
    private int    m_PinStep = 0;
    private string m_PinOld  = "";
    private string m_PinNew  = "";
    private bool   m_HasPin  = false;

    void OZ_PdaMenu()
    {
        m_Pages = new map<string, ref OZ_PdaPage>();
        m_Tabs  = new array<Widget>();
    }

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_menu.layout");
        if (!layoutRoot)
            return null;

        m_TabRail   = layoutRoot.FindAnyWidget("TabRail");
        m_PageHost  = layoutRoot.FindAnyWidget("PageHost");
        m_LockPanel = layoutRoot.FindAnyWidget("LockPanel");
        m_InitPanel = layoutRoot.FindAnyWidget("InitPanel");

        TextWidget itx = TextWidget.Cast(layoutRoot.FindAnyWidget("InitTitle"));
        if (itx)
            itx.SetText("#STR_OZ_INIT_TITLE");
        TextWidget ihx = TextWidget.Cast(layoutRoot.FindAnyWidget("InitHint"));
        if (ihx)
            ihx.SetText("#STR_OZ_INIT_HINT");
        TextWidget ibx = TextWidget.Cast(layoutRoot.FindAnyWidget("BtnInitBigText"));
        if (ibx)
            ibx.SetText("#STR_OZ_DEV_INIT");
        m_BtnClose  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnClose"));

        TextWidget ftx = TextWidget.Cast(layoutRoot.FindAnyWidget("BtnFactoryText"));
        if (ftx)
            ftx.SetText("#STR_OZ_FACTORY_RESET");

        return layoutRoot;
    }

    // Пастка, на якій горіли інші моди: якщо layout не завантажився, синглтон
    // меню лишається «відкритим» і блокує ВСІ меню гри назавжди. Тому перша ж
    // дія при показі -- перевірити, що дерево взагалі є.
    override void OnShow()
    {
        super.OnShow();

        if (!GetLayoutRoot())
        {
            OZ_Log.Error("pda layout failed to load - closing to avoid a ghost menu");
            GetGame().GetUIManager().CloseMenu(OZ_PdaConst.MENU_PDA);
            return;
        }

        SetFocus(layoutRoot);

        array<string> excludes = new array<string>();
        excludes.Insert("menu");
        GetGame().GetMission().AddActiveInputExcludes(excludes);

        // ЗАПАМ'ЯТОВУЄМО, ЯК БУЛО, і повертаємо саме це.
        //
        // OnHide безумовно вмикав ванільний інтерфейс назад -- тобто гравець,
        // який сам його сховав (клавіша HUD), після кожного закриття КПК
        // отримував його назад і мусив ховати знову.
        m_HudWasShown = HudWasShown();
        GetGame().GetMission().GetHud().Show(false);

        OZ_ClientState.BindListener(new OZ_PdaMenuListener(this));

        // Рушій може віддати ТОЙ САМИЙ примірник меню на наступному
        // відкритті -- FindMenu перевіряється саме тому. Отже ВЕСЬ стан, що
        // описує пристрій і незавершену дію, а не вікно, треба скидати тут: у
        // руках цілком може бути вже інший КПК.
        //
        // Скидався сам лише m_Sealed. Половина набору піна лишалась від
        // минулого відкриття: клавіатура поверталась у тому ж режимі, з тим
        // же недобраним буфером, і перша ж цифра доводила його до кінця --
        // на ІНШОМУ приладі.
        m_Sealed = false;
        m_PinMode   = "";
        m_PinStep   = 0;
        m_PinBuffer = "";
        m_PinOld    = "";
        m_PinNew    = "";

        // Питаємо сервер, що в нас за пристрій. До відповіді стрічка порожня.
        OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "status", "{}");

        if (!m_Refresh)
            m_Refresh = new Timer(CALL_CATEGORY_GUI);
        m_Refresh.Run(1.0, this, "RefreshTick", NULL, true);
    }

    override void OnHide()
    {
        super.OnHide();

        if (m_Refresh)
            m_Refresh.Stop();

        // Відкладене закриття могло не встигнути: вікно закрилось раніше
        // іншим шляхом. Парність CallLater/Remove -- правило, а не обережність.
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(this.Close);

        // ДЕМОНТАЖ СТОРІНОК -- ТУТ, і це не прибирання заради прибирання.
        // Сторінка контактів відписується від статичного інвокера у своєму
        // Unlink() -- але той Unlink мусить хтось покликати. Ніхто не кликав:
        // кожен цикл відкрити/закрити лишав живу сторінку, підписану на
        // відповіді, з мертвими віджетами в руках. Знайшов аудит, а не краш
        // -- крашем воно стало б у першого, хто відкриє КПК двічі й отримає
        // відповідь на роль.
        DropPages();

        OZ_ClientState.BindListener(null);

        // Режим «без предмета» живе рівно один показ: наступне відкриття
        // дією на справжньому приладі не має його успадкувати.
        OZ_PdaMenuGate.ClearVirtual();

        array<string> excludes = new array<string>();
        excludes.Insert("menu");
        GetGame().GetMission().RemoveActiveInputExcludes(excludes, true);
        GetGame().GetMission().GetHud().Show(m_HudWasShown);
    }

    // Закритись НАСТУПНИМ КАДРОМ. Для тих, кого кличуть зсередини чужого
    // обходу -- обробника відповіді, ітерації по сторінках, -- і кому не
    // можна руйнувати себе під ногами в того, хто кличе.
    private void CloseLater()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.Close, 0, false);
    }

    // Чи ванільний інтерфейс був увімкнений ДО того, як ми його сховали.
    //
    // Питаємо прапорці контексту, а не сам віджет: панель захована ззовні
    // (клавіша HUD, непритомність, відкрите чуже вікно) саме через них, і
    // IngameHud.Cast -- той самий шлях, яким до них ходить ваниль.
    private bool HudWasShown()
    {
        IngameHud hud = IngameHud.Cast(GetGame().GetMission().GetHud());
        if (!hud)
            return true;

        IngameHudVisibility vis = hud.GetHudVisibility();
        if (!vis)
            return true;

        // По одній перевірці на рядок: умова `if` у Enforce мусить уміщатися
        // в один рядок, перенесення дає "Expected ')', not a '||'".
        if (vis.IsContextFlagActive(EHudContextFlags.HUD_HIDE))
            return false;
        if (vis.IsContextFlagActive(EHudContextFlags.HUD_DISABLE))
            return false;

        return true;
    }

    // Кеш для тикових рішень: коли востаннє питали і що бачили netsync.
    private int  m_LastStatusAskMs = 0;
    private int  m_LastSealedAskMs = 0;
    private bool m_SawOn = false;
    private bool m_SawUnlocked = false;
    private bool m_CrackLive = false;

    void RefreshTick()
    {
        // ПРИЛАД ПІШОВ -- ЕКРАН ІДЕ ЗА НИМ.
        //
        // Меню жило далі: КПК можна було кинути на землю, віддати, втратити
        // разом із тілом -- а відкрите вікно лишалось на екрані й далі
        // питало сервер. Сервер, звісно, відмовляв (ворота дивляться на те,
        // що в руках і в слоті), тож на екрані лишалась остання картинка
        // чужого вже приладу, і гравець читав з неї те, чого більше не має.
        //
        // Питаємо ТОЙ САМИЙ Device(), яким користуються ворота: правило про
        // «мій прилад» одне на весь мод.
        //
        // Виняток один -- «КПК без предмета» (D132): за цим екраном сутності
        // немає з самого початку, і порожні руки для нього не подія.
        if (!OZ_PdaHud.Device() && !OZ_PdaMenuGate.Virtual())
        {
            OZ_Log.Dbg("pda: the device left the player, closing the screen");
            CloseLater();
            return;
        }

        // Годинник і заряд -- ЛОКАЛЬНІ: час світу і netsync-поле предмета.
        // Це й прибирає трафік, і чинить заморозку статус-бара на вкладках,
        // які статус не питають.
        LocalStatusTick();

        // Поки стрічки немає, оновлювати нема кому: замкнений пристрій не
        // віддав жодної сторінки. Питаємо стан самі -- інакше відімкнення
        // іншим шляхом (скинули пін, минув час) лишилось би непоміченим, і
        // екран коду висів би над уже відкритим пристроєм.
        if (!m_Built)
        {
            // ЗАПЕЧАТАНИЙ пристрій status не віддає взагалі: гейт пропускає
            // тільки операції замка (OZ_PdaAccess.IsLockOp), тож цей запит
            // повертається відмовою ГАРАНТОВАНО. Питати його щосекунди --
            // це просити те, у чому зобов'язані відмовити: один сеанс дав
            // 1423 такі відмови, по одній на секунду на гравця, і кожна
            // коштувала повного проходу через ворота на сервері.
            //
            // Перший тік питає все одно, і мусить: доти невідомо, пристрій
            // запечатаний чи просто замкнений, а звичайному замкненому саме
            // status і будує стрічку, щойно код приймуть. Щойно відповідь
            // `sealed` скаже «так», питання припиняються -- і поновлюються
            // самі, коли злам добіжить і та сама відповідь скаже «ні».
            // Замок і живлення -- netsync на предметі: флip видно локально
            // й миттєво, а страховочний запит лишається раз на 5 секунд
            // (пристрій могли відімкнути шляхом, якого netsync не покриє).
            bool askNow = false;
            OZ_PDA_Base dev = OZ_PdaHud.Device();
            if (dev)
            {
                bool on  = dev.OZ_IsOn();
                bool unl = dev.OZ_IsUnlocked();
                if (on != m_SawOn || unl != m_SawUnlocked)
                    askNow = true;
                m_SawOn = on;
                m_SawUnlocked = unl;
            }

            int nowMs = GetGame().GetTime();
            if (!m_Sealed && (askNow || nowMs - m_LastStatusAskMs >= 5000))
            {
                m_LastStatusAskMs = nowMs;
                OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "status", "{}");
            }

            // Поки йде злам, відлік мусить рухатись щосекунди -- гравець
            // дивиться на нього. Без зламу екран коду статичний, і сталого
            // опиту не заслуговує.
            if (m_PinMode == "unlock")
            {
                if (m_CrackLive || nowMs - m_LastSealedAskMs >= 5000)
                {
                    m_LastSealedAskMs = nowMs;
                    AskSealed();
                }
            }
            return;
        }

        if (m_Current != "" && m_Pages.Contains(m_Current))
            m_Pages.Get(m_Current).OnRefresh();

        OZ_PdaPage mate = Companion();
        if (mate)
            mate.OnRefresh();
    }

    // ----------------------------------------------------------- відповіді

    void HandleResponse(string pageId, string op, bool ok, string json, string error)
    {
        // Стрічку будує ПЕРША ж відповідь про пристрій -- і тільки один раз.
        if (!m_Built && pageId == OZ_PdaConst.PAGE_DEVICE && op == "status" && ok)
            BuildFrom(json);
        else if (m_Built && pageId == OZ_PdaConst.PAGE_DEVICE && op == "status" && ok)
            RebuildIfPagesChanged(json);

        if (pageId == OZ_PdaConst.PAGE_DEVICE && op == "status")
        {
            ApplyLockState(ok, json, error);
            PaintStatusBar(ok, json);
        }

        if (m_Pages.Contains(pageId))
            m_Pages.Get(pageId).OnResponse(op, ok, json, error);

        if (pageId == OZ_PdaConst.PAGE_DEVICE && op == "unlock")
        {
            if (ok)
            {
                // Код прийнято. Стрічки ще немає -- її будує ПЕРША вдала
                // відповідь про стан, і попросити її мусимо саме тут: доти
                // сторінок немає, а отже нема кому питати.
                EndPin();
                OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "status", "{}");
            }
            else
            {
                OnBadPin(error);
            }
        }

        if (pageId == OZ_PdaConst.PAGE_DEVICE && op == "sealed" && ok)
        {
            PaintSealed(json);
            return;
        }

        if (pageId == OZ_PdaConst.PAGE_DEVICE && op == "crack")
        {
            if (!ok)
                PaintPinPrompt("#" + error);
            AskSealed();
            return;
        }

        if (pageId == OZ_PdaConst.PAGE_DEVICE && op == "initiate")
        {
            if (ok)
            {
                // Стрічка вкладок разова, а в новій сесії їх більше:
                // закриваємось, наступне відкриття збере повний набір.
                //
                // НАСТУПНИМ КАДРОМ, а не тут. Close() у UIScriptedMenu --
                // proto native: він руйнує меню негайно, а нас саме зараз
                // кличе OZ_ClientState зсередини свого розбору відповіді й
                // після повернення звертається до себе далі. Знищувати
                // об'єкт із його ж зворотного виклику -- звернення по
                // мертвому вказівнику, і те, що воно досі не впало, нічого
                // не обіцяє.
                CloseLater();
            }
            return;
        }

        if (pageId == OZ_PdaConst.PAGE_DEVICE && op == "factory_reset")
        {
            if (ok)
            {
                // Пристрій щойно став чистим і відімкненим: екран коду геть,
                // стан перепитуємо -- стрічка збудується з нього.
                EndPin();
                OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "status", "{}");
            }
            else
            {
                PaintPinPrompt("#" + error);
            }
            return;
        }

        if (pageId == OZ_PdaConst.PAGE_DEVICE && op == "setpin")
        {
            if (ok)
            {
                // Код прийнято -- екран іде геть, а стан перепитує сама
                // сторінка: писати «пін задано» з голови клієнта означало б
                // показати те, чого сервер міг і не зробити.
                EndPin();
                OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "status", "{}");
            }
            else
            {
                OnBadPin(error);
            }
        }
    }

    // НАБІР СТОРІНОК МІНЯЄТЬСЯ ЗА ЖИТТЯ ВІКНА, і стрічка мусить за ним іти.
    //
    // Сторінку вмикає не лише профіль, а й ВСТАВЛЕНИЙ МОДУЛЬ: дістав рацію з
    // відсіку -- вкладка рації мусить зникнути, вставив -- з'явитись. Стрічка
    // ж будувалась рівно один раз, першою відповіддю, і після цього не
    // мінялась ніколи: гравець міняв залізо, дивлячись на вкладки, яких у
    // приладі вже немає. Натискання на таку вкладку йшло на сервер і чесно
    // отримувало відмову, якої ніхто не пояснював.
    //
    // Прилад сам штовхає стан на кожне під'єднання й від'єднання
    // (OZ_PDA_Base.EEItemAttached -> PushState), тож окремого опиту не треба.
    private void RebuildIfPagesChanged(string json)
    {
        string err;
        OZ_PdaDeviceStatus st;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.LoadData(json, st, err))
            return;
        if (!st.Pages)
            return;

        // Порядок теж значущий: його задає профіль, і зміна порядку -- це
        // інша стрічка.
        bool same = st.Pages.Count() == m_PageOrder.Count();
        if (same)
        {
            for (int i = 0; i < st.Pages.Count(); i++)
            {
                if (st.Pages[i] != m_PageOrder[i])
                {
                    same = false;
                    break;
                }
            }
        }

        if (same)
            return;

        // Що було відкрито -- лишаємо відкритим, якщо воно ще є.
        string keep = m_Current;

        DropPages();
        BuildFrom(json);

        if (keep != "" && m_Pages.Contains(keep))
            Select(keep);
    }

    // Знести сторінки й стрічку, лишивши вікно живим. Спільне для
    // перезбирання й для закриття -- інакше друге місце неминуче забуде
    // покликати Unlink, і сторінка лишиться підписаною з мертвими віджетами.
    private void DropPages()
    {
        if (m_Pages)
        {
            for (int i = 0; i < m_Pages.Count(); i++)
            {
                OZ_PdaPage page = m_Pages.GetElement(i);
                if (page)
                    page.Unlink();
            }
            m_Pages.Clear();
        }

        if (m_Tabs)
        {
            for (int t = 0; t < m_Tabs.Count(); t++)
            {
                if (m_Tabs[t])
                    m_Tabs[t].Unlink();
            }
            m_Tabs.Clear();
        }

        m_PageOrder.Clear();
        m_Current = "";
        m_Built = false;
    }

    // Стрічка, як її збудували: за нею й звіряємось.
    private ref array<string> m_PageOrder = new array<string>();

    private void BuildFrom(string json)
    {
        string err;
        OZ_PdaDeviceStatus st;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.LoadData(json, st, err))
        {
            OZ_Log.Error("device status unreadable while building tabs: " + err);
            return;
        }

        // ЗАЩІПКА СТАВИТЬСЯ В КІНЦІ, і лише коли стрічка справді з'явилась.
        //
        // Тут вона стояла на початку й безумовно. Профіль без сторінок (чи
        // такий, жодну сторінку якого цей клієнт малювати не вміє) давав
        // порожню стрічку -- і разом із нею глушив опит стану в RefreshTick,
        // бо той працює рівно доти, доки m_Built == false. Вікно лишалось
        // порожнім назавжди й перепитати вже не могло.

        // ФРАКЦІЯ ЖИВЕ В ОДНІЙ ВКЛАДЦІ З КОНТАКТАМИ (рішення власника
        // 2026-08-30): ліворуч люди, праворуч свої. Це те саме питання --
        // «хто навколо і чиї вони», -- і розводити його на дві вкладки
        // означало клацати між ними на кожну думку.
        //
        // Сторінки лишаються ДВІ: два серверні обробники, два конверти, два
        // незалежні оновлення. Спільна в них тільки вкладка -- і рівно це
        // тут і зроблено, без злиття коду сторінок в одну купу.
        //
        // Пару оголошує ТОЙ, ХТО ЇЇ УТВОРЮЄ (OZ_PdaPageFactory.Pair), а не
        // це меню. Тут стояло ім'я фракційної сторінки -- тобто КПК знав про
        // мод, якого може й не бути; після виносу фракцій окремим модом таке
        // знання стало прямою залежністю на порожнє місце.
        //
        // Якщо профіль дав одну сторінку пари без другої, вона отримує власну
        // вкладку, як і раніше: приліпити її нема до чого.
        m_Companion = "";
        for (int c = 0; c < st.Pages.Count(); c++)
        {
            string mate = OZ_PdaPageFactory.CompanionOf(st.Pages[c]);
            if (mate != "" && st.Pages.Find(mate) != -1)
            {
                m_Companion = mate;
                break;
            }
        }

        // Порядок задає ПРОФІЛЬ, не реєстр: адмін вирішує, що йде першим.
        m_PageOrder.Clear();
        for (int i = 0; i < st.Pages.Count(); i++)
        {
            AddTab(st.Pages[i]);
            m_PageOrder.Insert(st.Pages[i]);
        }

        // Жодної сторінки не вийшло -- нічого й не защіпаємо: хай опит іде
        // далі, а в лозі лишається причина.
        if (m_Pages.Count() == 0)
        {
            OZ_Log.Warn("pda: the device status brought no page this client can draw - the tab rail stays empty");
            return;
        }

        m_Built = true;

        if (st.Pages.Count() > 0)
            Select(st.Pages[0]);
    }

    // Сторінка, що ділить вкладку з контактами. Порожньо -- ділити нема чому.
    private string m_Companion = "";

    // Чия це вкладка. Для сторінки-супутника -- вкладка контактів.
    private string TabOf(string pageId)
    {
        if (m_Companion != "" && pageId == m_Companion)
            return OZ_PdaConst.PAGE_CONTACTS;
        return pageId;
    }

    private void AddTab(string pageId)
    {
        OZ_PdaPage page = OZ_PdaPageFactory.Make(pageId);
        if (!page)
            return;   // клієнт не вміє малювати -- вкладки не буде, причина в лозі

        // Супутник отримує сторінку, але НЕ вкладку: його показує та сама
        // кнопка, що й контакти.
        if (pageId != m_Companion)
        {
            Widget tab = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_tab.layout", m_TabRail);
            if (!tab)
                return;

            tab.SetName(pageId);
            tab.SetUserID(1);         // так OnClick відрізняє вкладку від решти

            TextWidget glyph = TextWidget.Cast(tab.FindAnyWidget("TabGlyph"));
            if (glyph)
                glyph.SetText(OZ_PdaPageFactory.Glyph(pageId));

            m_Tabs.Insert(tab);
        }

        page.Init(pageId, m_PageHost);
        page.Show(false);
        m_Pages.Insert(pageId, page);
    }

    void Select(string pageId)
    {
        // Обрати супутник -- це обрати вкладку, у якій він живе. Інакше
        // «перейди на фракцію» лишило б половину екрана порожньою: вкладки
        // з таким іменем немає, а пара, яку вона показує, сховалась би.
        if (m_Companion != "" && pageId == m_Companion)
            pageId = TabOf(pageId);

        if (m_Current == pageId)
            return;

        // Сторінки, якої немає, не існує й вибір: інакше поточна ховається,
        // нова не показується, і КПК стоїть порожнім екраном.
        if (!m_Pages.Contains(pageId))
            return;

        if (m_Current != "" && m_Pages.Contains(m_Current))
        {
            m_Pages.Get(m_Current).Show(false);
            m_Pages.Get(m_Current).OnDeselected();
        }

        // Супутник ховається разом зі своєю парою -- і показується разом.
        if (m_Companion != "" && m_Pages.Contains(m_Companion))
        {
            if (TabOf(m_Companion) == TabOf(m_Current))
            {
                m_Pages.Get(m_Companion).Show(false);
                m_Pages.Get(m_Companion).OnDeselected();
            }
        }

        m_Current = pageId;

        if (m_Pages.Contains(pageId))
        {
            m_Pages.Get(pageId).Show(true);
            m_Pages.Get(pageId).OnSelected();
        }

        if (m_Companion != "" && m_Pages.Contains(m_Companion))
        {
            if (TabOf(m_Companion) == TabOf(pageId) && m_Companion != pageId)
            {
                m_Pages.Get(m_Companion).Show(true);
                m_Pages.Get(m_Companion).OnSelected();
            }
        }

        PaintTabs();
    }

    // Сторінка на ім'я, або null. Потрібна сусідці по вкладці: фракція
    // питає контакти, кого там вибрано, і саме тому вони разом.
    OZ_PdaPage PageOf(string pageId)
    {
        if (!m_Pages || !m_Pages.Contains(pageId))
            return null;
        return m_Pages.Get(pageId);
    }

    // Сторінка-супутник поточної, або порожньо. Одна відповідь на питання
    // «кого ще стосується те, що зараз на екрані»: оновлення, кліки, миша.
    private OZ_PdaPage Companion()
    {
        if (m_Companion == "" || m_Current == "")
            return null;
        if (m_Companion == m_Current)
            return null;
        if (TabOf(m_Companion) != TabOf(m_Current))
            return null;
        if (!m_Pages.Contains(m_Companion))
            return null;
        return m_Pages.Get(m_Companion);
    }

    private void PaintTabs()
    {
        for (int i = 0; i < m_Tabs.Count(); i++)
        {
            Widget t = m_Tabs[i];
            bool active = (t.GetName() == m_Current);

            Widget mark = t.FindAnyWidget("TabActive");
            if (mark)
                mark.Show(active);

            TextWidget glyph = TextWidget.Cast(t.FindAnyWidget("TabGlyph"));
            if (glyph)
            {
                if (active)
                    glyph.SetColor(ARGB(255, 13, 13, 15));
                else
                    glyph.SetColor(ARGB(255, 93, 93, 99));
            }
        }
    }

    // ---------------------------------------------------------------- замок

    private void ApplyLockState(bool ok, string json, string error)
    {
        if (!m_LockPanel)
            return;

        // Відмова саме через замок -- єдина причина показати екран коду.
        //
        // І це ЗВИЧАЙНИЙ шлях, а не крайній випадок: замкнений пристрій не
        // віддає навіть device/status, тож нормально ми потрапляємо сюди, а
        // не в розбір відповіді нижче. Показати саму панель мало -- треба
        // ввімкнути режим, інакше цифри нікуди не йдуть: панель видно, а
        // кнопки мовчать. Саме так це й виглядало на живому клієнті.
        if (!ok)
        {
            // Замок -- ЄДИНА причина показати екран коду. Сервер відмовляє
            // status'у на замкненому пристрої саме кодом STR_OZ_ERR_LOCKED
            // (OZ_PdaAccess: status не входить у IsLockOp). Раніше клієнт
            // чекав тут NO_ACCESS -- код, який на замок НЕ приходить, тож
            // екран коду не з'являвся ЖОДНОГО разу, а прилад без піна над
            // яким висів NO_ACCESS (немає девайса зовсім) навпаки діставав
            // незакриваний пад. Тепер матчимо саме LOCKED.
            if (error == "STR_OZ_ERR_LOCKED")
            {
                if (m_PinMode != "unlock")
                    BeginPin("unlock");

                // Запечатаний пристрій показує СВОЄ. Пропонувати набирати код
                // там, де його ніхто не знає, -- це запрошення в нікуди.
                AskSealed();
            }
            else if (m_PinMode == "unlock")
            {
                EndPin();
            }
            return;
        }

        string err;
        OZ_PdaDeviceStatus st;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.LoadData(json, st, err))
            return;

        m_HasPin = st.HasPin;

        // Нічийний пристрій -- ЕКРАН ІНІЦІАЦІЇ замість сторінок: після
        // factory reset (чи зі свіжим приладом) єдина доступна дія --
        // зробити його своїм.
        ShowInit(!st.Owned && st.Powered && st.Unlocked);

        // Вимкнений прилад не має екрана коду -- і НАБОРУ теж: буфер і
        // крок пережили б вимикання, і наступні цифри доклеювались би до
        // мертвого стану (зміряно живим тестом 2026-08-29: «зміна коду»
        // з двох сеансів набору). Вимкнули -- набір скінчився.
        if (!st.Powered)
        {
            if (m_PinMode != "")
                EndPin();
            return;
        }

        bool needPin = st.HasPin && !st.Unlocked;

        if (needPin)
        {
            // Примусовий екран б'є будь-який добровільний: замкнений пристрій
            // не місце для зміни коду.
            if (m_PinMode != "unlock")
                BeginPin("unlock");

            TextWidget hint = TextWidget.Cast(layoutRoot.FindAnyWidget("LockHint"));
            if (hint)
            {
                if (st.LockedOut)
                {
                    string wait = Widget.TranslateString("#STR_OZ_LOCK_TOO_MANY");
                    if (st.LockWaitS > 0)
                        wait += "  (" + st.LockWaitS.ToString() + " s)";
                    hint.SetText(wait);
                }
                else
                    hint.SetText("#STR_OZ_PIN_HINT");
            }
            return;
        }

        // Відімкнули -- примусовий екран іде геть. Добровільний лишається:
        // гравець сам його відкрив і сам закриє.
        if (m_PinMode == "unlock")
            EndPin();
    }

    // ------------------------------------------------------------ екран коду

    // Просить сторінка «Пристрій» -- через FindMenu, а не через посилання:
    // меню одне, живе в UIManager, і тримати на нього другу нитку означало б
    // мати два джерела правди про те, чи воно взагалі відкрите.
    private void ShowInit(bool show)
    {
        m_InitShown = show;
        SyncPanels();
    }

    // ОДНА ТОЧКА, ЯКА ВИРІШУЄ ПРО ВСІ ЧОТИРИ ВІДЖЕТИ.
    //
    // Видимість панелі ініціалізації, екрана коду, поля сторінок і стрічки
    // вкладок пов'язана: рівно одна з перших двох може бути на екрані, і поки
    // хоч одна з них є -- сторінок бути не може. Це правило жило в трьох
    // місцях по шматку (ShowInit, BeginPin, EndPin), і кожне знало лише
    // частину: EndPin, наприклад, безумовно вмикав поле сторінок -- зокрема й
    // поверх відкритої панелі ініціалізації.
    //
    // Стан описують ДВІ змінні, а малює їх ця функція. Хто міняє стан --
    // кличе її й більше нічого не показує сам.
    private bool m_InitShown = false;

    private void SyncPanels()
    {
        bool pin = m_PinMode != "";

        if (m_InitPanel)
            m_InitPanel.Show(m_InitShown);

        if (m_LockPanel)
            m_LockPanel.Show(pin);

        // Сторінки ховаємо ОКРЕМО, а не покладаємось на те, що панель їх
        // перекриє: 3D-прев'ю предмета малюється власним проходом рушія і
        // проступає крізь будь-який 2D-віджет поверх нього. Виміряно на
        // живому клієнті -- модель було видно просто крізь екран коду.
        if (m_PageHost)
            m_PageHost.Show(!m_InitShown && !pin);

        if (m_TabRail)
            m_TabRail.Show(!m_InitShown);
    }

    void BeginPin(string mode)
    {
        if (!m_LockPanel)
            return;

        m_PinMode   = mode;
        m_PinBuffer = "";
        m_PinOld    = "";
        m_PinNew    = "";

        // Задати код там, де його ще немає, -- це одразу новий код, без
        // питання про старий.
        if (mode == "set" && !m_HasPin)
            m_PinStep = 1;
        else
            m_PinStep = 0;

        SyncPanels();

        PaintPinPrompt("");
        PaintPinDots();

        // Скидання до заводських -- лише на ПРИМУСОВОМУ екрані коду: там
        // стоїть той, хто коду не знає. Добровільні set/clear -- власник.
        ButtonWidget fac = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnFactory"));
        if (fac)
            fac.Show(mode == "unlock");
    }

    // Запечатаний пристрій не віддає навіть status, тож про його стан
    // доводиться питати ОКРЕМО -- операцією, яку гейт пропускає.
    private void AskSealed()
    {
        OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "sealed", "{}");
    }

    // Запечатаний пристрій має СВОЮ розповідь, і клавіатура в ній не бере
    // участі: набирати код, якого ніхто не знає, нема сенсу.
    private void PaintSealed(string json)
    {
        string err;
        OZ_PdaDeviceStatus st;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.LoadData(json, st, err))
            return;

        // Єдине місце, де цей прапорець ставиться. Коли злам добігає, сервер
        // прибирає код, пристрій перестає бути запечатаним, і та сама
        // відповідь сама ж поверне сюди false -- після чого RefreshTick знову
        // спитає status і збудує стрічку. Окремого сигналу «зламано» не
        // треба саме тому.
        m_Sealed = st.Sealed;

        // Поки йде злам, RefreshTick питає sealed щосекунди (живий відлік);
        // без зламу -- раз на 5 секунд, екран коду статичний.
        m_CrackLive = st.Cracking;

        ButtonWidget crack = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnCrack"));
        Widget pad = layoutRoot.FindAnyWidget("LockPad");

        if (!st.Sealed)
        {
            // Звичайний замкнений КПК: код і панель як були, але з
            // дешифратором у гнізді замок ламається й тут (рішення власника
            // 2026-08-28) -- DECRYPT стоїть під падом, поруч зі скиданням.
            if (pad)
                pad.Show(true);

            TextWidget hintP = TextWidget.Cast(layoutRoot.FindAnyWidget("LockHint"));

            if (st.Cracking)
            {
                if (crack)
                    crack.Show(false);
                if (hintP)
                {
                    string leftP = "#STR_OZ_CRACKING";
                    leftP += "   " + st.CrackLeftSec.ToString() + " s";
                    hintP.SetText(leftP);
                }
                return;
            }

            if (crack)
            {
                crack.Show(st.HasDecryptor);
                if (st.HasDecryptor)
                {
                    TextWidget ctP = TextWidget.Cast(layoutRoot.FindAnyWidget("BtnCrackText"));
                    if (ctP)
                        ctP.SetText("#STR_OZ_CRACK");
                }
            }
            return;
        }

        if (pad)
            pad.Show(false);

        TextWidget label = TextWidget.Cast(layoutRoot.FindAnyWidget("LockLabel"));
        if (label)
            label.SetText("#STR_OZ_SEALED");

        TextWidget dots = TextWidget.Cast(layoutRoot.FindAnyWidget("LockDots"));
        if (dots)
            dots.SetText("");

        TextWidget hint = TextWidget.Cast(layoutRoot.FindAnyWidget("LockHint"));

        if (st.Cracking)
        {
            if (crack)
                crack.Show(false);
            if (hint)
            {
                string left = "#STR_OZ_CRACKING";
                left += "   " + st.CrackLeftSec.ToString() + " s";
                hint.SetText(left);
            }
            return;
        }

        if (!st.HasDecryptor)
        {
            if (crack)
                crack.Show(false);
            if (hint)
                hint.SetText("#STR_OZ_SEALED_NEED");
            return;
        }

        if (crack)
        {
            crack.Show(true);
            TextWidget ct = TextWidget.Cast(layoutRoot.FindAnyWidget("BtnCrackText"));
            if (ct)
                ct.SetText("#STR_OZ_CRACK");
        }
        if (hint)
            hint.SetText("");
    }

    void EndPin()
    {
        m_PinMode   = "";
        m_PinStep   = 0;
        m_PinBuffer = "";
        m_PinOld    = "";
        m_PinNew    = "";

        SyncPanels();

        // Панель вимкнули -- усе, що жило лише на ній, теж. Інакше кнопка
        // зламу лишалась би видимою на вже відкритому пристрої.
        ButtonWidget crack = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnCrack"));
        if (crack)
            crack.Show(false);

        ButtonWidget fac2 = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnFactory"));
        if (fac2)
            fac2.Show(false);

        Widget pad = layoutRoot.FindAnyWidget("LockPad");
        if (pad)
            pad.Show(true);
    }

    bool PinPanelBusy()
    {
        return m_PinMode != "";
    }

    bool HasPin()
    {
        return m_HasPin;
    }

    private void PaintPinPrompt(string hintKey)
    {
        TextWidget label = TextWidget.Cast(layoutRoot.FindAnyWidget("LockLabel"));
        if (label)
        {
            if (m_PinMode == "unlock")
                label.SetText("#STR_OZ_LOCK_PROMPT");
            else if (m_PinStep == 0)
                label.SetText("#STR_OZ_PIN_OLD");
            else if (m_PinStep == 1)
                label.SetText("#STR_OZ_PIN_NEW");
            else
                label.SetText("#STR_OZ_PIN_REPEAT");
        }

        TextWidget hint = TextWidget.Cast(layoutRoot.FindAnyWidget("LockHint"));
        if (hint)
        {
            if (hintKey != "")
                hint.SetText(hintKey);
            else
                hint.SetText("#STR_OZ_PIN_HINT");
        }
    }

    // Enter натиснуто. Що це означає -- залежить від режиму й кроку.
    private void PinConfirm()
    {
        if (m_PinMode == "unlock")
        {
            OZ_PdaPinAttempt att = new OZ_PdaPinAttempt();
            att.Pin = m_PinBuffer;

            string ajson;
            string aerr;
            if (JsonFileLoader<OZ_PdaPinAttempt>.MakeData(att, ajson, aerr, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "unlock", ajson);
            return;
        }

        if (m_PinMode == "clear")
        {
            m_PinOld = m_PinBuffer;
            SendPinChange(m_PinOld, "");
            return;
        }

        // m_PinMode == "set"
        if (m_PinStep == 0)
        {
            m_PinOld    = m_PinBuffer;
            m_PinBuffer = "";
            m_PinStep   = 1;
            PaintPinPrompt("");
            PaintPinDots();
            return;
        }

        if (m_PinStep == 1)
        {
            m_PinNew    = m_PinBuffer;
            m_PinBuffer = "";
            m_PinStep   = 2;
            PaintPinPrompt("");
            PaintPinDots();
            return;
        }

        // Повтор не збігся -- повертаємось на крок назад, а не мовчки
        // приймаємо перший варіант.
        if (m_PinBuffer != m_PinNew)
        {
            m_PinNew    = "";
            m_PinBuffer = "";
            m_PinStep   = 1;
            PaintPinPrompt("#STR_OZ_PIN_MISMATCH");
            PaintPinDots();
            return;
        }

        SendPinChange(m_PinOld, m_PinNew);
    }

    private void SendPinChange(string oldPin, string newPin)
    {
        OZ_PdaPinChange ch = new OZ_PdaPinChange();
        ch.OldPin = oldPin;
        ch.NewPin = newPin;

        string json;
        string err;
        if (JsonFileLoader<OZ_PdaPinChange>.MakeData(ch, json, err, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "setpin", json);
    }

    // Смуга стану -- те, що гравець мусить бачити, не заходячи на сторінку:
    // живлення, стан зв'язку й час. Ті самі дані, що вже прийшли; окремого
    // запиту не робимо.
    // Локальна половина статус-бара: заряд із netsync-поля предмета й
    // годинник світу. Сервер тут ні до чого.
    private void LocalStatusTick()
    {
        TextWidget left  = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusLeft"));
        TextWidget right = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusRight"));

        OZ_PDA_Base dev = OZ_PdaHud.Device();

        // Без предмета заряду немає, і брехати про нього нема чого: у смузі
        // стоїть ім'я режиму (D132).
        if (left && !dev && OZ_PdaMenuGate.Virtual())
            left.SetText("#STR_OZ_DEV_VIRTUAL");

        if (left && dev)
        {
            if (!dev.OZ_IsOn())
            {
                left.SetText("#STR_OZ_DEV_OFF");
            }
            else
            {
                int pct = Math.Round(dev.OZ_Charge01() * 100);
                string l = "#STR_OZ_DEV_POWER";
                l += "  " + pct.ToString() + "%";
                left.SetText(l);
            }
        }

        if (right)
        {
            int y, mo, d, h, m;
            GetGame().GetWorld().GetDate(y, mo, d, h, m);
            string tm = Pad2(h);
            tm += ":" + Pad2(m);
            right.SetText(tm);
        }
    }

    private void PaintStatusBar(bool ok, string json)
    {
        TextWidget left  = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusLeft"));
        TextWidget mid   = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusMid"));
        TextWidget right = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusRight"));

        if (!ok)
        {
            if (left)  left.SetText("#STR_OZ_DEV_OFF");
            if (mid)   mid.SetText("");
            if (right) right.SetText("");
            return;
        }

        string err;
        OZ_PdaDeviceStatus st;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.LoadData(json, st, err))
            return;

        if (left)
        {
            if (!st.Powered)
                left.SetText("#STR_OZ_DEV_OFF");
            else
            {
                int pct = Math.Round(st.Charge01 * 100);
                string l = "#STR_OZ_DEV_POWER";
                l += "  " + pct.ToString() + "%";
                left.SetText(l);
            }
        }

        if (mid)
        {
            if (st.Online)
                mid.SetText("");
            else
                mid.SetText("#STR_OZ_DEV_OFFLINE_SHORT");
        }

        if (right)
        {
            // Час беремо ігровий: гравцеві потрібен час Зони, а не свій
            // системний.
            // GetHours/GetMinutes не існує -- рушій віддає дату цілком одним
            // викликом: GetDate(out year, month, day, hour, minute).
            int y, mo, d, h, m;
            GetGame().GetWorld().GetDate(y, mo, d, h, m);
            string tm = Pad2(h);
            tm += ":" + Pad2(m);
            right.SetText(tm);
        }
    }

    private string Pad2(int v)
    {
        if (v < 10)
            return "0" + v.ToString();
        return v.ToString();
    }

    // true -- натискання було по цифровій панелі й уже розібране.
    private bool PinPadClick(string name)
    {
        if (name == "KeyOk")
        {
            if (m_PinBuffer.Length() > 0)
                PinConfirm();
            return true;
        }

        if (name == "KeyBack")
        {
            if (m_PinBuffer.Length() > 0)
            {
                m_PinBuffer = m_PinBuffer.Substring(0, m_PinBuffer.Length() - 1);
                PaintPinDots();
            }
            return true;
        }

        // Key0..Key9 -- і тільки вони: чуже ім'я тут не цифра.
        if (name.Length() != 4)
            return false;
        if (name.Substring(0, 3) != "Key")
            return false;

        string digit = name.Substring(3, 1);
        if (digit != "0" && digit.ToInt() == 0)
            return false;

        if (m_PinBuffer.Length() < 4)
        {
            m_PinBuffer += digit;
            PaintPinDots();
        }
        return true;
    }

    private void PaintPinDots()
    {
        TextWidget dots = TextWidget.Cast(layoutRoot.FindAnyWidget("LockDots"));
        if (!dots)
            return;

        string s = "";
        for (int i = 0; i < 4; i++)
        {
            if (i < m_PinBuffer.Length())
                s += "*";
            else
                s += "-";
        }
        dots.SetText(s);
    }

    // Причину каже СЕРВЕР, і показувати треба саме її. Раніше тут завжди
    // писалось «Wrong code», і через це «пристрою немає» -- гравець помер, а
    // КПК лишився на трупі -- виглядало як невірний код. Півгодини пішло на
    // те, щоб зрозуміти, що вводити нема куди.
    private void OnBadPin(string error)
    {
        m_PinBuffer = "";
        PaintPinDots();

        // На кроці «повтори новий код» помилятись нема в чому -- сервер
        // відмовляє лише через СТАРИЙ код, тож повертаємо на його крок.
        if (m_PinMode == "set" && m_PinStep > 0 && m_HasPin)
        {
            m_PinStep = 0;
            m_PinOld  = "";
            m_PinNew  = "";
        }

        string why = "#STR_OZ_LOCK_WRONG";
        if (error != "")
            why = "#" + error;
        PaintPinPrompt(why);
    }

    override bool OnKeyPress(Widget w, int x, int y, int key)
    {
        if (key == KeyCode.KC_ESCAPE)
        {
            // Добровільний екран коду скасовується; примусовий -- ні, бо з
            // замкненого пристрою виходити нема куди, крім як із меню.
            if (m_PinMode != "" && m_PinMode != "unlock")
            {
                EndPin();
                return true;
            }

            Close();
            return true;
        }

        // Код набирається лише поки відкритий екран коду.
        if (m_PinMode != "")
        {
            if (key >= KeyCode.KC_0 && key <= KeyCode.KC_9 && m_PinBuffer.Length() < 4)
            {
                m_PinBuffer += (key - KeyCode.KC_0).ToString();
                PaintPinDots();
                return true;
            }

            if (key == KeyCode.KC_BACK && m_PinBuffer.Length() > 0)
            {
                m_PinBuffer = m_PinBuffer.Substring(0, m_PinBuffer.Length() - 1);
                PaintPinDots();
                return true;
            }

            if (key == KeyCode.KC_RETURN && m_PinBuffer.Length() > 0)
            {
                PinConfirm();
                return true;
            }
        }

        return super.OnKeyPress(w, x, y, key);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_BtnClose)
        {
            Close();
            return true;
        }

        if (w && w.GetUserID() == 1)
        {
            Select(w.GetName());
            return true;
        }

        if (m_PinMode != "" && w && w.GetName() == "BtnCrack")
        {
            OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "crack", "{}");
            return true;
        }

        if (w && w.GetName() == "BtnInitBig")
        {
            OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "initiate", "{}");
            return true;
        }

        if (m_PinMode != "" && w && w.GetName() == "BtnFactory")
        {
            OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "factory_reset", "{}");
            return true;
        }

        // Цифрова панель екрана коду. Розбирається ТУТ, а не на сторінці:
        // набраний код живе в меню й ніде більше.
        if (m_PinMode != "" && w && PinPadClick(w.GetName()))
            return true;

        // Далі -- активна сторінка й та, що ділить із нею вкладку. Решту не
        // питаємо: сторінки, якої не видно, клікнути неможливо, і давати їй
        // голос означало б ловити чужі кнопки.
        if (m_Current != "" && m_Pages.Contains(m_Current))
        {
            if (m_Pages.Get(m_Current).OnPageClick(w, x, y))
                return true;
        }

        OZ_PdaPage mate = Companion();
        if (mate && mate.OnPageClick(w, x, y))
            return true;

        return super.OnClick(w, x, y, button);
    }

    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (m_Current != "" && m_Pages.Contains(m_Current))
        {
            if (m_Pages.Get(m_Current).OnPageMouseDown(w, x, y))
                return true;
        }

        OZ_PdaPage mate = Companion();
        if (mate && mate.OnPageMouseDown(w, x, y))
            return true;

        return super.OnMouseButtonDown(w, x, y, button);
    }

    // Відпускання лівої так само їде на сторінку: клік по MapWidget не
    // породжує OnClick, і карта збирає його з пари down+up сама.
    override bool OnMouseButtonUp(Widget w, int x, int y, int button)
    {
        if (button == MouseState.LEFT && m_Current != "" && m_Pages.Contains(m_Current))
        {
            if (m_Pages.Get(m_Current).OnPageMouseUp(w, x, y))
                return true;

            OZ_PdaPage mate = Companion();
            if (mate && mate.OnPageMouseUp(w, x, y))
                return true;
        }

        return super.OnMouseButtonUp(w, x, y, button);
    }

    override bool OnItemSelected(Widget w, int x, int y, int row, int column, int oldRow, int oldColumn)
    {
        if (m_Current != "" && m_Pages.Contains(m_Current))
        {
            if (m_Pages.Get(m_Current).OnPageItemSelected(w, row))
                return true;
        }

        OZ_PdaPage mate = Companion();
        if (mate && mate.OnPageItemSelected(w, row))
            return true;

        return super.OnItemSelected(w, x, y, row, column, oldRow, oldColumn);
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (w && w.GetUserID() == 1)
        {
            Widget hover = w.FindAnyWidget("TabHover");
            if (hover)
                hover.Show(true);
            return true;
        }
        return super.OnMouseEnter(w, x, y);
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (w && w.GetUserID() == 1)
        {
            Widget hover = w.FindAnyWidget("TabHover");
            if (hover)
                hover.Show(false);
            return true;
        }
        return super.OnMouseLeave(w, enterW, x, y);
    }
}

// Тонкий перехідник: ядро кличе слухача, слухач кличе меню. Меню не може
// успадкувати OZ_ResponseListener саме, бо воно вже UIScriptedMenu.
class OZ_PdaMenuListener : OZ_ResponseListener
{
    private OZ_PdaMenu m_Menu;

    void OZ_PdaMenuListener(OZ_PdaMenu menu)
    {
        m_Menu = menu;
    }

    override void OnResponse(string pageId, string op, bool ok, string json, string error)
    {
        if (m_Menu)
            m_Menu.HandleResponse(pageId, op, ok, json, error);
    }
}
