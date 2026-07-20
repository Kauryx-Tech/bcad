# Politique de sécurité

bcad est une application CAO de bureau qui lit/écrit des fichiers locaux
(DXF, projets SQLite `.bcad`). La principale surface de risque est un
fichier d'entrée malveillant ou malformé déclenchant un bug de sécurité
mémoire dans les analyseurs (`io::DxfReader`, `io::Database`) ou dans le
code géométrique adossé à CGAL.

## Signaler une vulnérabilité

Merci de signaler les problèmes de sécurité en privé plutôt que d'ouvrir
une issue publique : via les
[GitHub Security Advisories](https://github.com/Kauryx-Tech/bcad/security/advisories/new)
de ce dépôt, ou par e-mail à contact@kauryxgroup.com.

Merci d'inclure, si possible : le fichier ou l'entrée qui déclenche le
problème, le module concerné, et le commit/tag testé.

Ce projet est personnel/petite équipe et n'a pas de délai de réponse
garanti (SLA), mais les signalements seront pris en compte et traités
aussi vite que raisonnablement possible.
