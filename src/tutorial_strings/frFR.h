#pragma once
#include "tutorial_text.h"

// French (frFR): the tutorial's words.
namespace D2RAccess::TutorialText::Tables {

inline constexpr Texts frFR{
	.welcome = {
		"Tutoriel de D2R Access. Je vais vous montrer comment vous repérer avec le mod jusqu'à la fin de la première quête. "
		"Chaque étape vous demande de faire quelque chose ; quand c'est fait, le son d'étape terminée retentit et la suivante arrive. "
		"{view} et {rs} à gauche ou à droite parcourent ces messages ; {view} et {up} reviennent au premier. "
		"Vous pouvez désactiver le tutoriel dans la liste des réglages, avec {view} et {rb}.",
		"Tutoriel de D2R Access. Je vais vous montrer comment vous repérer avec le mod jusqu'à la fin de la première quête. "
		"Chaque étape vous demande de faire quelque chose ; quand c'est fait, le son d'étape terminée retentit et la suivante arrive. "
		"K et L parcourent ces messages ; Maj plus K revient au premier. "
		"Vous pouvez désactiver le tutoriel dans la liste des réglages, avec Ctrl plus S."},
	.steps = {
		{"Première étape : votre état. Appuyez sur {view} et {x} pour entendre votre vie et votre mana.",
			"Première étape : votre état. Appuyez sur Z pour entendre votre vie, et sur X pour votre mana."},
		{"Le radar sonne dans quatre directions quand votre environnement change ; vers le bas, il sonne plus grave. "
			"Je vais maintenant jouer ses sons, chacun après son nom et sa signification.",
			"Le radar sonne dans quatre directions quand votre environnement change ; vers le bas, il sonne plus grave. "
			"Je vais maintenant jouer ses sons, chacun après son nom et sa signification."},
		{"En plus du radar, les éléments proches sonnent depuis l'endroit où ils se trouvent, plus fort quand ils "
			"sont plus près : les personnes, les lieux et ce qui se trouve au sol, les objets avec les sons que vous "
			"avez choisis dans la liste des réglages. Jusqu'ici je les ai gardés discrets pour que vous entendiez le "
			"radar seul. Je vais jouer chacun après son nom.",
			"En plus du radar, les éléments proches sonnent depuis l'endroit où ils se trouvent, plus fort quand ils "
			"sont plus près : les personnes, les lieux et ce qui se trouve au sol, les objets avec les sons que vous "
			"avez choisis dans la liste des réglages. Jusqu'ici je les ai gardés discrets pour que vous entendiez le "
			"radar seul. Je vais jouer chacun après son nom."},
		{"Le scanner. Il liste ce qui se trouve sur la carte par catégorie : sorties, PNJ, points de passage et plus, le plus proche en "
			"premier, avec la distance et la direction. Avec {rs}, haut et bas changent la catégorie, et gauche et droite la parcourent. "
			"Trouvez Warriv dans la catégorie des PNJ : c'est le plus proche de vous.",
			"Le scanner. Il liste ce qui se trouve sur la carte par catégorie : sorties, PNJ, points de passage et plus, le plus proche en "
			"premier, avec la distance et la direction. Ctrl plus Page précédente et Ctrl plus Page suivante changent la catégorie ; Page précédente et "
			"Page suivante la parcourent. Trouvez Warriv dans la catégorie des PNJ : c'est le plus proche de vous."},
		{"Parlez à Warriv. Il est à quelques pas : avancez vers lui avec {ls} comme l'a dit le scanner, appuyez sur {a} à côté de "
			"lui et écoutez ce qu'il a à vous dire. Quand il a fini, fermez le dialogue avec {b}.",
			"Parlez à Warriv. Il est à quelques pas : avancez vers lui avec les touches fléchées comme l'a dit le scanner, appuyez sur "
			"D à côté de lui et écoutez ce qu'il a à vous dire. Quand il a fini, fermez le dialogue avec Échap."},
		{"Maintenant Akara, la guérisseuse du camp : elle a la première quête. Elle est plus loin ; trouvez-la dans la catégorie "
			"des PNJ du scanner.",
			"Maintenant Akara, la guérisseuse du camp : elle a la première quête. Elle est plus loin ; trouvez-la dans la catégorie "
			"des PNJ du scanner."},
		{"L'itinéraire. Il vous indique le chemin vers ce que le scanner a nommé, en directions et en distances, comme vous le planifieriez "
			"sur la minicarte ; il ne marche pas à votre place. Une fois Akara nommée, appuyez sur {route}.",
			"L'itinéraire. Il vous indique le chemin vers ce que le scanner a nommé, en directions et en distances, comme vous le planifieriez "
			"sur la minicarte ; il ne marche pas à votre place. Une fois Akara nommée, appuyez sur Ctrl plus E."},
		{"Le guidage. Il émet des signaux vers ce que le scanner a nommé et vous conduit sur le chemin, "
			"mais c'est vous qui marchez. Appuyez sur {view} et {lb} pour suivre Akara, et marchez vers les signaux jusqu'à l'atteindre ; "
			"un nouvel appui le relâche.",
			"Le guidage. Il émet des signaux vers ce que le scanner a nommé et vous conduit sur le chemin, "
			"mais c'est vous qui marchez. Appuyez sur Ctrl plus B pour suivre Akara, et marchez avec les touches fléchées vers les signaux jusqu'à "
			"l'atteindre ; un nouvel appui le relâche."},
		{"Parlez à Akara : à côté d'elle, appuyez sur {a}. Elle a une quête pour vous. "
			"Quand elle a fini, fermez le dialogue avec {b}.",
			"Parlez à Akara : à côté d'elle, appuyez sur D. Elle a une quête pour vous. "
			"Quand elle a fini, fermez le dialogue avec Échap."},
		{"Hors du camp, il y a des monstres, et chaque espèce sonne à sa manière depuis l'endroit où elle se trouve. "
			"Avant de partir, je vais les jouer, chacun après son nom.",
			"Hors du camp, il y a des monstres, et chaque espèce sonne à sa manière depuis l'endroit où elle se trouve. "
			"Avant de partir, je vais les jouer, chacun après son nom."},
		{"Zone actuelle : {moor}. En extérieur, deux questions vous situent : {view} et {b} donnent un aperçu du "
			"terrain, et {view} et {rs} vers le haut, la description de la zone. Essayez l'aperçu.",
			"Zone actuelle : {moor}. Ctrl plus O vous donne un aperçu du terrain autour de vous. Essayez-le."},
		{"Il y a des monstres ici : ils sonnent comme vous les avez entendus au camp, et le scanner les liste dans "
			"leur propre catégorie. Attaquez avec {a} ; vos autres compétences sont sur les boutons que vous leur avez "
			"donnés. Le double bip de visée indique que votre attaque est dirigée vers un monstre : attaquez quand vous "
			"l'entendez, et s'il reste silencieux, tournez-vous vers le grognement jusqu'à ce qu'il retentisse, pour "
			"ne gaspiller aucune attaque sur un mur ou un tonneau. Si votre vie baisse, buvez une potion avec {dpad}. Ce qu'ils laissent tomber sonne depuis l'endroit "
			"où cela se trouve : approchez-vous et appuyez sur {a} pour le ramasser ; l'or se ramasse en marchant "
			"dessus. Tuez un monstre.",
			"Il y a des monstres ici : ils sonnent comme vous les avez entendus au camp, et le scanner les liste dans "
			"leur propre catégorie. Tab choisit le monstre suivant ; maintenez A pour attaquer avec votre compétence "
			"gauche, et W pour la droite. Le double bip de visée indique que votre attaque est dirigée vers un monstre : "
			"attaquez quand vous l'entendez, pour ne gaspiller aucune attaque sur un mur ou un tonneau. Si votre vie "
			"baisse, les potions sont sur les touches 1 à 4. Ce qu'ils "
			"laissent tomber sonne depuis l'endroit où cela se trouve : point choisit l'objet suivant au sol, virgule le "
			"précédent, et D le ramasse ; l'or se ramasse en marchant dessus. Tuez un monstre."},
		{"Trouvez {den}. La carte est refaite à chaque partie, donc son entrée peut être proche ou lointaine. Suivez "
			"les sons du radar, ou les tons des murs si vous les avez activés : les passages inexplorés et le sol "
			"inexploré mènent à du terrain nouveau, et le radar vous prévient quand il note une nouvelle sortie, que le "
			"scanner liste alors parmi ses sorties. Cherchez-y {den} : une fois dans la liste, l'itinéraire et le "
			"guidage vous mènent à son entrée. Si vous vous perdez, la catégorie des zones inexplorées du scanner vous "
			"montre où aller.",
			"Trouvez {den}. La carte est refaite à chaque partie, donc son entrée peut être proche ou lointaine. Suivez "
			"les sons du radar, ou les tons des murs si vous les avez activés : les passages inexplorés et le sol "
			"inexploré mènent à du terrain nouveau, et le radar vous prévient quand il note une nouvelle sortie, que le "
			"scanner liste alors parmi ses sorties. Cherchez-y {den} : une fois dans la liste, l'itinéraire et le "
			"guidage vous mènent à son entrée. Si vous vous perdez, la catégorie des zones inexplorées du scanner vous "
			"montre où aller."},
		{"Zone actuelle : {den}. Dans les grottes et les donjons, le mod connaît les salles : {view} et {y} disent "
			"les issues de la salle où vous êtes. Essayez maintenant.",
			"Zone actuelle : {den}. Dans les grottes et les donjons, le mod connaît les salles : Ctrl plus Y dit les "
			"issues de la salle où vous êtes. Essayez maintenant."},
		{"En entrant dans chaque salle, vous entendrez si des parties en sont encore inexplorées. Tuez tous les "
			"monstres dans {den} ; le jeu dit combien il en reste. La catégorie des zones inexplorées du scanner vous "
			"aide à n'en oublier aucun.",
			"En entrant dans chaque salle, vous entendrez si des parties en sont encore inexplorées. Tuez tous les "
			"monstres dans {den} ; le jeu dit combien il en reste. La catégorie des zones inexplorées du scanner vous "
			"aide à n'en oublier aucun."},
		{"Plus aucun monstre dans {den}. Retournez au camp : ouvrez l'inventaire, utilisez le parchemin de portail de "
			"retour en ville, et entrez dans le portail avec {a}. Vous pouvez aussi revenir à pied avec l'itinéraire et "
			"le guidage.",
			"Plus aucun monstre dans {den}. Retournez au camp : ouvrez l'inventaire avec I, allez au parchemin de "
			"portail de retour en ville et appuyez sur D pour le lire ; fermez l'inventaire et appuyez sur D à côté du "
			"portail. Vous pouvez aussi revenir à pied avec l'itinéraire et le guidage."},
		{
			"Parlez à Akara pour votre récompense, un point de compétence : à côté d'elle, appuyez sur {a}.",
			"Parlez à Akara pour votre récompense, un point de compétence : à côté d'elle, appuyez sur D."},
		{
			"Akara vend aussi des livres, qui économisent de la place dans l'inventaire. Dans son menu, "
			"choisissez {trade} avec {dpad} puis appuyez sur {a} ; si vous l'avez fermé, parlez-lui de nouveau. À "
			"l'intérieur, je vous explique comment acheter.",
			"Akara vend aussi des livres, qui économisent de la place dans l'inventaire. Dans son menu, "
			"choisissez {trade} avec les touches fléchées puis appuyez sur A ; si vous l'avez fermé, parlez-lui "
			"de nouveau avec D. À l'intérieur, je vous explique comment acheter."},
		{
			"Dépensez maintenant votre point de compétence : ouvrez l'arbre de compétences avec {menu}. LT et RT "
			"changent de branche, {dpad} parcourt ses compétences, et maintenir {a} dépense le point sur celle où "
			"vous êtes. Quand vous avez fini, fermez-le avec {b}.",
			"Dépensez maintenant votre point de compétence : ouvrez l'arbre de compétences avec T. Page "
			"précédente et Page suivante changent de branche, les touches fléchées parcourent ses compétences, et "
			"A dépense le point sur celle où se trouve le curseur. Quand vous avez fini, fermez-le avec Échap."},
		{
			"Encore une chose que le jeu ne vous dit pas : les points de passage gardent votre chemin. Chaque "
			"fois que vous revenez jouer, vous commencez au camp, et une zone lointaine s'atteint par son point "
			"de passage, qui n'est à vous qu'une fois touché. Le camp a le sien : trouvez-le dans la catégorie "
			"des points de passage du scanner, demandez l'itinéraire, suivez le guidage et appuyez sur {a} à côté "
			"pour ouvrir son menu.",
			"Encore une chose que le jeu ne vous dit pas : les points de passage gardent votre chemin. Chaque "
			"fois que vous revenez jouer, vous commencez au camp, et une zone lointaine s'atteint par son point "
			"de passage, qui n'est à vous qu'une fois touché. Le camp a le sien : trouvez-le dans la catégorie "
			"des points de passage du scanner, demandez l'itinéraire, suivez le guidage et appuyez sur D à côté "
			"pour ouvrir son menu."},
		{"Tutoriel terminé. Vous savez vous repérer : scanner, itinéraire, guidage et radar. Dans la plupart des "
			"menus, {rs} vers le bas dit ce que font les boutons : essayez-le dans chaque menu nouveau pour vous, comme "
			"le coffre personnel. Dans la liste des réglages, avec {view} et {rb}, vous choisissez ce que le mod dit et "
			"joue. À partir de maintenant, le jeu est à vous.",
			"Tutoriel terminé. Vous savez vous repérer : scanner, itinéraire, guidage et radar. Dans la liste des "
			"réglages, avec Ctrl plus S, vous choisissez ce que le mod dit et joue. Le clavier a aussi des raccourcis "
			"qui marchent à votre place : H va à la zone inexplorée la plus proche, et Maj plus Origine à ce que le "
			"scanner a nommé. À partir de maintenant, le jeu est à vous."}},
	.radarTask = {
		"Marchez maintenant avec {ls} jusqu'à entendre un mur.",
		"Marchez maintenant avec les touches fléchées jusqu'à entendre un mur."},
	.cues = {"Passage : une ouverture qui mène à une autre salle ou à une sortie",
		"Passage inexploré : le passage avec deux bips à la fin ; il mène à un terrain que vous n'avez pas encore vu",
		"Sol libre : vous pouvez marcher dans cette direction ; plus loin il peut y avoir n'importe quoi, un passage aussi",
		"Sol inexploré : le sol avec les deux bips ; un terrain que vous n'avez pas encore vu se trouve dans cette direction", "Mur : il vous barre la route",
		"Porte : elle s'ouvre pour vous laisser passer", "Petit obstacle : quelque chose de petit sur votre chemin que vous pouvez contourner"},
	.worldCues = {"PNJ, quelqu'un à qui parler",
		"PNJ avec un point d'exclamation : il a quelque chose pour vous",
		"Coffre personnel, le coffre où vous gardez vos objets",
		"Escalier, entrée ou portail",
		"Bord de la zone, le passage vers une autre zone",
		"Zone inexplorée",
		"De l'or au sol",
		"Une potion au sol",
		"Un objet au sol que vous pouvez porter, comme une arme"},
	.worldTask = {
		"Maintenant, ceux qui vous entourent vont sonner. Si vous ne savez pas ce qu'est un son, le scanner vous dit ce qui se trouve là, "
		"et chaque son peut être désactivé dans la liste des réglages.",
		"Maintenant, ceux qui vous entourent vont sonner. Si vous ne savez pas ce qu'est un son, le scanner vous dit ce qui se trouve là, "
		"et chaque son peut être désactivé dans la liste des réglages."},
	.enemyCues = {"Monstre : les plus proches grognent depuis l'endroit où ils se trouvent",
		"Monstre qui attaque de loin, comme un archer",
		"Chaman qui ressuscite les monstres morts : tuez-le en premier",
		"Élite : un champion, un monstre unique ou un boss",
		"À portée : un monstre est à portée de votre attaque",
		"Visée : votre attaque est dirigée vers un monstre que vous voyez"},
	.leaveTask = {
		"Maintenant quittez le camp par vous-même, avec ce que vous avez appris : parmi les sorties du scanner, "
		"trouvez {moor}, demandez l'itinéraire, suivez le guidage et franchissez la sortie.",
		"Maintenant quittez le camp par vous-même, avec ce que vous avez appris : parmi les sorties du scanner, "
		"trouvez {moor}, demandez l'itinéraire, suivez le guidage et franchissez la sortie."},
	.auditionAgain = {
		"Ils se suivent ; vous n'avez aucune touche à presser. Pour réentendre un son, appuyez sur {view} et {rs} à "
		"gauche : le suivant attend alors jusqu'à {view} et {rs} à droite. Plus tard, en revenant en arrière dans "
		"ces messages, chaque son est rejoué.",
		"Ils se suivent ; vous n'avez aucune touche à presser. Pour réentendre un son, appuyez sur K : le suivant "
		"attend alors jusqu'à L. Plus tard, en revenant en arrière dans ces messages, chaque son est rejoué."},
	.auditionWait = {
		"Après chaque son, {view} et {rs} à gauche le rejouent, et {view} et {rs} à droite passent au suivant. Plus "
		"tard, en revenant en arrière dans ces messages, chaque son est rejoué.",
		"Après chaque son, K le rejoue, et L passe au suivant. Plus tard, en revenant en arrière dans ces messages, "
		"chaque son est rejoué."},
	.portalButton = {
		"Plus rapide : {portal} ouvre aussitôt un portail de retour en ville, sans passer par l'inventaire, tant que "
		"vous portez un parchemin ou un tome de portail de retour en ville.",
		"Plus rapide : {portal} ouvre aussitôt un portail de retour en ville, sans passer par l'inventaire, tant que "
		"vous portez un parchemin ou un tome de portail de retour en ville."},
	.gold = {
		"Vous avez ramassé de l'or ; il se ramasse en marchant dessus. Si vous mourez, vous perdez une partie de "
		"l'or que vous portez, mais l'or du coffre est à l'abri. De retour au camp, ouvrez le coffre personnel et "
		"déposez ce dont vous n'avez pas besoin.",
		"Vous avez ramassé de l'or ; il se ramasse en marchant dessus. Si vous mourez, vous perdez une partie de "
		"l'or que vous portez, mais l'or du coffre est à l'abri. De retour au camp, ouvrez le coffre personnel et "
		"déposez ce dont vous n'avez pas besoin."},
	.shop = {
		"Une boutique. Ce qu'elle vend est d'un côté, dans des onglets que LT et RT changent, et votre "
		"inventaire de l'autre. {dpad} va de case en case, et {rs} vers le haut lit la description de "
		"l'objet. Cherchez un {portaltome} et un {idtome}, qu'Akara vend ; sur l'un d'eux, appuyez sur {a} et "
		"confirmez pour l'acheter, si vous avez l'or. Chacun contient jusqu'à 20 parchemins sur un "
		"emplacement d'une colonne de large et deux rangées de haut, et les parchemins de son type que vous "
		"achetez ensuite y vont ; les parchemins isolés prennent une case chacun et remplissent vite "
		"l'inventaire. Un parchemin d'identification montre ce qu'est un objet non identifié : utilisez-le, "
		"puis choisissez l'objet. Fermez la boutique avec {b}."
		" Même si vous n'achetez rien, le tutoriel continue ; les tomes peuvent attendre.",
		"Une boutique. Ce qu'elle vend est d'un côté, dans des onglets que Page précédente et Page suivante "
		"changent, et votre inventaire de l'autre. Les touches fléchées amènent le curseur de case en case, "
		"et J lit la description de l'objet. Cherchez un {portaltome} et un {idtome}, qu'Akara vend ; sur "
		"l'un d'eux, appuyez sur D pour l'acheter, si vous avez l'or. Chacun contient jusqu'à 20 parchemins "
		"sur un emplacement d'une colonne de large et deux rangées de haut, et les parchemins de son type que "
		"vous achetez ensuite y vont ; les parchemins isolés prennent une case chacun et remplissent vite "
		"l'inventaire. Un parchemin d'identification montre ce qu'est un objet non identifié : utilisez-le, "
		"puis choisissez l'objet. Fermez la boutique avec Échap."
		" Même si vous n'achetez rien, le tutoriel continue ; les tomes peuvent attendre."},
	.levelUp = {
		"Vous avez gagné un niveau : vous avez des points de caractéristiques et un point de compétence à "
		"dépenser. Appuyez sur {menu} : votre écran de personnage ou votre arbre de compétences s'ouvre, et "
		"je vous guiderai à l'intérieur.",
		"Vous avez gagné un niveau : vous avez des points de caractéristiques et un point de compétence à "
		"dépenser. Appuyez sur C pour ouvrir votre écran de personnage, ou sur T pour l'arbre de compétences "
		"; je vous guiderai à l'intérieur."},
	.levelUpSheet = {
		"Votre écran de personnage. Vous êtes sur le plus de la Force : {dpad} haut et bas parcourent les quatre caractéristiques, et "
		"maintenir {a} dépense un point sur celle où vous êtes ; maintenir {x} en dépense cinq d'un coup. {rs} haut et bas lisent vos statistiques ligne par ligne, et "
		"droite dit les points qu'il vous reste.",
		"Votre écran de personnage. Les touches fléchées amènent le curseur sur le plus de chaque caractéristique, et A y dépense un point. "
		"J lit la description de la ligne où vous êtes, et Maj avec la flèche haut ou bas la parcourt ligne par "
		"ligne."},
	.levelUpSwitch = {
		"Vos points de caractéristiques sont dépensés. Maintenant votre point de compétence : appuyez sur {rb} jusqu'à atteindre l'arbre de compétences ; chaque menu "
		"dit son nom.",
		"Vos points de caractéristiques sont dépensés. Maintenant votre point de compétence : appuyez sur T pour ouvrir l'arbre de compétences à côté de votre écran de personnage."},
	.levelUpTree = {
		"L'arbre de compétences. LT et RT changent de branche, {dpad} parcourt ses compétences, et {rs} vers "
		"le haut lit celle où vous êtes. Maintenir {a} y dépense le point, et {rs} vers la droite dit les "
		"points qu'il vous reste. Quand vous avez fini, fermez-le avec {b}.",
		"L'arbre de compétences. Page précédente et Page suivante changent de branche, les touches fléchées parcourent ses compétences, et J lit "
		"celle où se trouve le curseur. A y dépense le point, et Ctrl plus Page suivante dit les points qu'il vous "
		"reste. Quand vous avez fini, fermez-le avec Échap."},
	.levelUpTreeLater = {
		"Vous avez encore votre point de compétence. Appuyez sur {menu} : une fois les caractéristiques dépensées, il ouvre l'arbre de compétences.",
		"Vous avez encore votre point de compétence. Appuyez sur T pour ouvrir l'arbre de compétences."},
	.levelUpToSheet = {
		"Votre point de compétence est dépensé. Il vous reste des points de caractéristiques : appuyez sur "
		"{lb} jusqu'à atteindre votre écran de personnage ; chaque menu dit son nom.",
		"Votre point de compétence est dépensé. Il vous reste des points de caractéristiques : appuyez sur C "
		"pour ouvrir votre écran de personnage."},
	.levelUpSheetLater = {
		"Il vous reste des points de caractéristiques. Appuyez sur {menu} et allez avec {lb} ou {rb} jusqu'à "
		"votre écran de personnage ; chaque menu dit son nom.",
		"Il vous reste des points de caractéristiques. Appuyez sur C pour ouvrir votre écran de personnage."},
	.levelUpBind = {
		"Pour utiliser une compétence, placez-la sur un bouton : dans l'arbre de compétences, sur la compétence, "
		"appuyez sur {x} puis sur le bouton voulu. La lecture de chaque compétence dit sur quel bouton elle se "
		"trouve. Les compétences passives, comme les maîtrises, agissent d'elles-mêmes : elles n'ont besoin d'aucun "
		"bouton et ne peuvent pas en avoir. Avec LT et RT, après les branches de votre classe, la page {general} "
		"contient ce que tout personnage possède, comme Portail retour et Identifier ; vous les placez sur des "
		"boutons de la même façon.",
		"Pour utiliser une compétence, placez-la dans une main : S ouvre la liste des compétences pour la main "
		"droite, et Maj plus S celle pour la main gauche ; les touches fléchées en choisissent une et A la place "
		"dans cette main. Une liste ouverte, F1 à F8 associent la compétence sous le curseur à cette touche. Les "
		"compétences passives, comme les maîtrises, agissent d'elles-mêmes : elles ne figurent pas dans ces listes "
		"et n'ont besoin de rien d'autre."},
	.kit = {
		"Vous avez ramassé quelque chose à porter. Appuyez sur {menu} pour ouvrir les menus ; ils s'ouvrent sur votre inventaire, et je "
		"vous guiderai à l'intérieur.",
		"Vous avez ramassé quelque chose à porter. Appuyez sur I pour ouvrir votre inventaire ; je vous guiderai à l'intérieur."},
	.kitOpen = {
		"Votre inventaire. {dpad} va de case en case, dans votre sac à dos et sur ce que vous portez, et le mod dit "
		"ce que contient chacune. Sur un objet, {rs} vers le haut lit sa description, et {rs} vers le bas les boutons que vous pouvez utiliser avec lui, "
		"dont celui qui l'équipe. Un objet qui dit non identifié ne peut pas être porté tant qu'il n'est pas identifié, "
		"avec un parchemin d'identification, qu'Akara vend.",
		"Votre inventaire. Les touches fléchées amènent le curseur de case en case, dans votre sac à dos et sur ce que vous "
		"portez, et le mod dit ce que contient chacune. J lit la description d'un objet, et Maj plus A l'équipe. Un objet "
		"qui dit non identifié ne peut pas être porté tant qu'il n'est pas identifié, avec un parchemin d'identification, qu'Akara vend."},
	.kitWorn = {
		"Vous le portez. Fermez l'inventaire avec {b} quand vous avez fini.",
		"Vous le portez. Fermez l'inventaire avec Échap quand vous avez fini."},
	.kitDone = {
		"Terminé. Quoi que vous ramassiez, l'inventaire est l'endroit où vous l'équipez. Retour à ce que vous faisiez :",
		"Terminé. Quoi que vous ramassiez, l'inventaire est l'endroit où vous l'équipez. Retour à ce que vous faisiez :"},
	.levelUpDone = {
		"Terminé. Essayez maintenant votre nouvelle compétence une fois : appuyez sur le bouton où vous l'avez "
		"placée et écoutez comment elle sonne. Chaque fois que vous gagnez un niveau, {menu} ouvre votre écran de "
		"personnage. Retour à ce que vous faisiez :",
		"Terminé. Essayez maintenant votre nouvelle compétence une fois : A utilise la compétence de la main gauche "
		"et W celle de la droite ; écoutez comment elle sonne. Chaque fois que vous gagnez un niveau, C ouvre votre "
		"écran de personnage et T l'arbre de compétences. Retour à ce que vous faisiez :"},
	.waypointMenu = {
		"Le menu du point de passage. {dpad} haut et bas parcourent les zones de l'acte qui en ont un : "
		"celles qui disent non découvert, vous ne les avez pas encore touchées, et celle qui dit vous êtes "
		"ici est celle-ci. Sur une zone découverte, {a} vous y emmène aussitôt ; LT et RT changent d'acte. "
		"Pour l'instant vous n'avez que celui-ci ; le suivant est dans {plains}, après {moor} : touchez-le, "
		"et dès lors vous irez de l'un à l'autre en un instant. Fermez le menu avec {b}.",
		"Le menu du point de passage. Les touches fléchées haut et bas parcourent les zones de l'acte qui en "
		"ont un : celles qui disent non découvert, vous ne les avez pas encore touchées, et celle qui dit "
		"vous êtes ici est celle-ci. Sur une zone découverte, A vous y emmène aussitôt ; Page précédente et "
		"Page suivante changent d'acte. Pour l'instant vous n'avez que celui-ci ; le suivant est dans "
		"{plains}, après {moor} : touchez-le, et dès lors vous irez de l'un à l'autre en un instant. Fermez "
		"le menu avec Échap."},
	.routeStick = "{view} et {rs} vers le bas",
	.routeButton = "{view} et {a}",
	.firstMessage = "Premier message",
	.lastMessage = "Dernier message",
	.den = "Le Repaire du Mal",
	.moor = "La Lande sanglante",
	.plains = "Les Plaines gelées",
	.portalTome = "Tome de portail de retour en ville",
	.identifyTome = "Tome d’identification",
	.generalSkills = "Compétences générales",
	.trade = "acheter/vendre",
	.offer = {
		"Voulez-vous suivre le tutoriel de D2R Access, qui vous apprend à utiliser le mod ? Appuyez deux fois sur {a} (croix sur PlayStation) pour le commencer, deux fois sur {b} (rond) pour pas maintenant, ou deux fois sur {y} (triangle) pour ne plus jamais être sollicité. Tant que vous n'avez pas répondu, le jeu ne reçoit aucune commande.",
		"Voulez-vous suivre le tutoriel de D2R Access, qui vous apprend à utiliser le mod ? Appuyez deux fois sur Entrée pour le commencer, deux fois sur Échap pour pas maintenant, ou deux fois sur Retour arrière pour ne plus jamais être sollicité. Tant que vous n'avez pas répondu, le jeu ne reçoit aucune commande."},
	.offerLater = {"Pas de tutoriel pour l'instant. Je vous le proposerai dans une prochaine partie.", "Pas de tutoriel pour l'instant. Je vous le proposerai dans une prochaine partie."},
	.offerNever = {
		"Je ne vous le proposerai plus. Pour retrouver le tutoriel, activez-le dans la liste des réglages, avec {view} et {rb}.",
		"Je ne vous le proposerai plus. Pour retrouver le tutoriel, activez-le dans la liste des réglages, avec Ctrl plus S."},
};
}
