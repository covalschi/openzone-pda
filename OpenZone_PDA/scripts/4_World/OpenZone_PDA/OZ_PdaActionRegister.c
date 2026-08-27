// Реєстрація власних дій у загальному списку.
//
// AddAction() на предметі -- це ще не все. Він каже, які дії має ЦЕЙ предмет,
// але сам список дій гри будує ActionConstructor: він роздає їм числові id,
// створює по одному примірнику на дію й тримає карту typename -> дія. Дія,
// якої немає в тому списку, не існує для рушія взагалі -- і AddAction на неї
// нічого не змінює.
//
// Саме тому OZ_ActionOpenPda був написаний давно й не працював жодного разу:
// входом завжди був хоткей, і ніхто не помітив, що дія не зареєстрована.
// Перевірка сказала прямо: «is a script class but is not registered as an
// action».

modded class ActionConstructor
{
    override void RegisterActions(TTypenameArray actions)
    {
        super.RegisterActions(actions);

        actions.Insert(OZ_ActionOpenPda);
        actions.Insert(OZ_ActionExchangeContacts);
    }
}
