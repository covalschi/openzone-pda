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
}
