// База сторінки КПК.
//
// Сторінка -- це віджет плюс дві звички: попросити дані, коли її відкрили, і
// прийняти відповідь, коли вона прийшла. Більше нічого вона не робить: усі
// рішення серверні, сторінка лише малює.
//
// Життєвий цикл: Init -> OnSelected -> (OnResponse ...) -> OnDeselected.
// Init кличеться один раз, OnSelected -- щоразу при переході на вкладку.

class OZ_PdaPage
{
    protected Widget m_Root;
    protected string m_PageId;

    // БЕЗ ІНІЦІАЛІЗАТОРІВ, і це не стиль, а вимога. Виміряно бісекцією на
    // стенді 2026-08-26, по одному полю за раз:
    //
    //     protected string m_StickyWidget;        -> компілюється
    //     protected string m_StickyWidget = "";   -> НЕ компілюється
    //
    // Ініціалізатор поля в класі, ВІД ЯКОГО УСПАДКОВУЮТЬСЯ, ламає доступ
    // нащадків до їхніх ВЛАСНИХ private-методів: усі чотири сторінки разом
    // отримали "Method 'Request' is private" на рядках, яких ніхто не чіпав,
    // а сам OZ_PdaPage.c у списку помилок не з'явився взагалі. Помилка
    // вказує не туди, де причина, тож без цього коментаря наступний
    // сеанс шукав би її в сторінках.
    //
    // У листовому класі (OZ_PdaPageNotes) ініціалізатор працює -- саме тому
    // в моді є і той, і той стиль. Enforce і так обнуляє поля: рядок стає
    // "", int -- 0, тобто ініціалізатор тут нічого й не додавав.
    protected string m_StickyWidget;
    protected int    m_StickyUntil;

    Widget Root()
    {
        return m_Root;
    }

    string PageId()
    {
        return m_PageId;
    }

    // Перевизначає нащадок: віддає шлях до свого layout.
    string LayoutPath()
    {
        return "";
    }

    void Init(string pageId, Widget parent)
    {
        m_PageId = pageId;

        string path = LayoutPath();
        if (path == "")
            return;

        m_Root = GetGame().GetWorkspace().CreateWidgets(path, parent);
        if (!m_Root)
        {
            OZ_Log.Error("page \"" + pageId + "\" layout failed to load: " + path);
            return;
        }

        OnBuilt();
    }

    // Кешувати посилання на віджети -- тут, після створення дерева.
    void OnBuilt()
    {
    }

    // Вкладку відкрили. Звідси й просять дані.
    void OnSelected()
    {
    }

    void OnDeselected()
    {
    }

    // Відповідь на запит ЦІЄЇ сторінки. Чужі сюди не доходять.
    void OnResponse(string op, bool ok, string json, string error)
    {
    }

    // Періодичне оновлення, поки вкладка відкрита. Раз на секунду, не щокадру.
    void OnRefresh()
    {
    }

    // Клік усередині сторінки. Меню віддає його ЛИШЕ активній сторінці й лише
    // після власних кнопок, тому чужі натискання сюди не доходять. Повертає
    // true, якщо взяла -- інакше меню передає клік далі.
    // Координати передаються РАЗОМ із віджетом: карті потрібна не лише
    // «клікнули по мені», а й куди саме -- без цього мітку нема де ставити.
    bool OnPageClick(Widget w, int x, int y)
    {
        return false;
    }

    // Вибір рядка в списку -- ОКРЕМА подія рушія, а не клік: TextListboxWidget
    // веде виділення сам і повідомляє про це через OnItemSelected. Ловити його
    // кліком означало б читати виділення до того, як воно змінилось.
    // НАТИСКАННЯ (не клік) над віджетом сторінки. Потрібне карті: клік
    // приходить після відпускання, і без точки натискання не відрізнити
    // «клацнув» від «потягнув карту й відпустив». Повертає false завжди,
    // коли подія не спожита -- рушію вона теж потрібна.
    bool OnPageMouseDown(Widget w, int x, int y)
    {
        return false;
    }

    bool OnPageItemSelected(Widget w, int row)
    {
        return false;
    }

    void Show(bool show)
    {
        if (m_Root)
            m_Root.Show(show);
    }

    void Unlink()
    {
        if (m_Root)
        {
            m_Root.Unlink();
            m_Root = null;
        }
    }

    // Дрібні помічники, щоб нащадки не повторювали Cast на кожному рядку.
    protected TextWidget Text(string name)
    {
        if (!m_Root)
            return null;
        return TextWidget.Cast(m_Root.FindAnyWidget(name));
    }

    protected Widget Wgt(string name)
    {
        if (!m_Root)
            return null;
        return m_Root.FindAnyWidget(name);
    }

    protected void SetText(string name, string value)
    {
        TextWidget w = Text(name);
        if (w)
            w.SetText(value);
    }

    // ------------------------------------------------------------ підказки
    //
    // Сторінки, які перепитують себе раз на секунду, ГУБИЛИ відповіді
    // сервера. Схема була одна на всіх: дія пише підказку, тут-таки кличе
    // Request(), а Paint() приїжджого списку затирає той самий віджет
    // безумовно. Між написанням і затиранням -- частка секунди, тобто
    // «міток більше не можна», відмова транспондера й відмова через живлення
    // не доходили до очей ЖОДНОГО разу.
    //
    // Правило просте: підказка від СЕРВЕРА липка й тримається кілька секунд,
    // а звичайна перемальовка їй поступається. Черга з підказок не потрібна
    // -- людина читає одну, і остання відмова важливіша за попередню.
    // Те, що сказав сервер. Затерти може лише інша липка підказка.
    protected void SetHintSticky(string name, string value)
    {
        SetText(name, value);
        m_StickyWidget = name;
        m_StickyUntil  = GetGame().GetTime() + OZ_PdaConst.HINT_HOLD_MS;
    }

    // Побутова підказка: поступається липкій, поки та не вийшла.
    protected void SetHint(string name, string value)
    {
        if (name == m_StickyWidget && GetGame().GetTime() < m_StickyUntil)
            return;

        if (name == m_StickyWidget)
            m_StickyWidget = "";

        SetText(name, value);
    }

    // Вкладку перемкнули або сторінку відкрили заново -- показувати відмову
    // на дію, якої гравець уже не пам'ятає, не треба.
    protected void ClearHintHold()
    {
        m_StickyWidget = "";
        m_StickyUntil  = 0;
    }
}
