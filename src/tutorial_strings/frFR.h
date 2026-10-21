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
		{"Le radar sonne dans quatre directions quand votre environnement change ; vers le bas, il sonne plus grave. Je vais maintenant jouer "
			"ses sons, chacun après son nom et sa signification ; vous n'avez aucune touche à presser.",
			"Le radar sonne dans quatre directions quand votre environnement change ; vers le bas, il sonne plus grave. Je vais maintenant jouer "
			"ses sons, chacun après son nom et sa signification ; vous n'avez aucune touche à presser."},
		{"En plus du radar, les éléments proches sonnent depuis l'endroit où ils se trouvent, plus fort quand ils sont plus près. Jusqu'ici je les ai "
			"gardés discrets pour que vous entendiez le radar seul. Je vais jouer chacun après son nom.",
			"En plus du radar, les éléments proches sonnent depuis l'endroit où ils se trouvent, plus fort quand ils sont plus près. Jusqu'ici je les ai "
			"gardés discrets pour que vous entendiez le radar seul. Je vais jouer chacun après son nom."},
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
		{"Maintenant quittez le camp par vous-même, avec ce que vous avez appris : parmi les sorties du scanner, trouvez {moor}, "
			"demandez l'itinéraire, suivez le guidage et franchissez la sortie.",
			"Maintenant quittez le camp par vous-même, avec ce que vous avez appris : parmi les sorties du scanner, trouvez {moor}, "
			"demandez l'itinéraire, suivez le guidage et franchissez la sortie."},
		{"Zone actuelle : {moor}. Trois questions vous situent : {view} et {y} disent les issues de la zone où vous êtes ; "
			"{view} et {b}, un aperçu du terrain ; {view} et {rs} vers le haut, la description de la zone. Essayez les deux premières.",
			"Zone actuelle : {moor}. Deux questions vous situent : Ctrl plus Y dit les issues de la zone où vous êtes, "
			"et Ctrl plus O, un aperçu du terrain. Essayez les deux."},
		{"Il y a des monstres ici. Ils grognent en approchant, et le scanner les liste dans leur propre catégorie. "
			"Attaquez avec {a} ; vos autres compétences sont sur les boutons que vous leur avez donnés. Si votre vie baisse, buvez une potion "
			"avec {dpad}. Tuez un monstre et ramassez l'or qu'il laisse : approchez-vous et appuyez sur {a}.",
			"Il y a des monstres ici. Ils grognent en approchant, et le scanner les liste dans leur propre catégorie. "
			"Tab choisit le monstre suivant ; maintenez A pour attaquer avec votre compétence gauche, et W pour la droite. Si votre vie "
			"baisse, les potions sont sur les touches 1 à 4. Tuez un monstre et ramassez son or : point choisit l'objet suivant "
			"au sol et virgule le précédent, et D le ramasse."},
		{"Trouvez {den}. La carte est refaite à chaque partie, donc son entrée peut être proche ou lointaine. Suivez les sons du "
			"radar, ou les tons des murs si vous les avez activés : les passages inexplorés et le sol inexploré mènent à du terrain nouveau, et le "
			"radar vous prévient quand il note une nouvelle sortie, que le scanner liste alors parmi ses sorties. Si vous vous perdez, "
			"la catégorie des salles inexplorées du scanner vous montre où aller. Entrez dans la grotte.",
			"Trouvez {den}. La carte est refaite à chaque partie, donc son entrée peut être proche ou lointaine. Suivez les sons du "
			"radar, ou les tons des murs si vous les avez activés : les passages inexplorés et le sol inexploré mènent à du terrain nouveau, et le "
			"radar vous prévient quand il note une nouvelle sortie, que le scanner liste alors parmi ses sorties. Si vous vous perdez, "
			"la catégorie des salles inexplorées du scanner vous montre où aller. Entrez dans la grotte."},
		{"Zone actuelle : {den}. En entrant dans chaque salle, vous entendrez si des parties en sont encore inexplorées. Tuez tous les "
			"monstres de la grotte ; le jeu dit combien il en reste. La catégorie des salles inexplorées du scanner vous aide à n'en oublier aucun.",
			"Zone actuelle : {den}. En entrant dans chaque salle, vous entendrez si des parties en sont encore inexplorées. Tuez tous les "
			"monstres de la grotte ; le jeu dit combien il en reste. La catégorie des salles inexplorées du scanner vous aide à n'en oublier aucun."},
		{"La grotte est nettoyée. Retournez au camp : ouvrez l'inventaire, utilisez le parchemin de portail de retour en ville, et entrez dans le "
			"portail avec {a}. Vous pouvez aussi revenir à pied avec l'itinéraire et le guidage.",
			"La grotte est nettoyée. Retournez au camp : ouvrez l'inventaire avec I, allez au parchemin de portail de retour en ville et appuyez sur D "
			"pour le lire ; fermez l'inventaire et appuyez sur D à côté du portail. Vous pouvez aussi revenir à pied avec l'itinéraire et "
			"le guidage."},
		{"Parlez à Akara pour votre récompense, un point de compétence. Ouvrez ensuite l'arbre de compétences avec {menu} et dépensez-le.",
			"Parlez à Akara pour votre récompense, un point de compétence. Ouvrez ensuite l'arbre de compétences avec T et dépensez-le."},
		{"Tutoriel terminé. Vous savez vous repérer : scanner, itinéraire, guidage et radar. Dans la liste des réglages, "
			"avec {view} et {rb}, vous choisissez ce que le mod dit et joue. À partir de maintenant, le jeu est à vous.",
			"Tutoriel terminé. Vous savez vous repérer : scanner, itinéraire, guidage et radar. Dans la liste des réglages, "
			"avec Ctrl plus S, vous choisissez ce que le mod dit et joue. Le clavier a aussi des raccourcis qui marchent "
			"à votre place : H va à la salle inexplorée la plus proche, et Maj plus Origine à ce que le scanner a nommé. "
			"À partir de maintenant, le jeu est à vous."}},
	.radarTask = {
		"Marchez maintenant avec {ls} jusqu'à entendre un mur.",
		"Marchez maintenant avec les touches fléchées jusqu'à entendre un mur."},
	.cues = {"Passage : une ouverture qui mène à une autre salle ou à une sortie",
		"Passage inexploré : le passage avec deux bips à la fin ; il mène à un terrain que vous n'avez pas encore vu",
		"Sol libre : vous pouvez marcher dans cette direction ; plus loin il peut y avoir n'importe quoi, un passage aussi",
		"Sol inexploré : le sol avec les deux bips ; un terrain que vous n'avez pas encore vu se trouve dans cette direction", "Mur : il vous barre la route",
		"Porte : elle s'ouvre pour vous laisser passer", "Petit obstacle : quelque chose de petit sur votre chemin que vous pouvez contourner"},
	.worldCues = {"PNJ, quelqu'un à qui parler", "PNJ avec un point d'exclamation : il a quelque chose pour vous",
		"Coffre personnel, le coffre où vous gardez vos objets", "Escalier, entrée ou portail", "Bord de la zone, le passage vers une autre zone",
		"Salle inexplorée"},
	.worldTask = {
		"Maintenant, ceux qui vous entourent vont sonner. Si vous ne savez pas ce qu'est un son, le scanner vous dit ce qui se trouve là, "
		"et chaque son peut être désactivé dans la liste des réglages.",
		"Maintenant, ceux qui vous entourent vont sonner. Si vous ne savez pas ce qu'est un son, le scanner vous dit ce qui se trouve là, "
		"et chaque son peut être désactivé dans la liste des réglages."},
	.levelUp = {
		"Vous avez gagné un niveau : vous avez des points de caractéristiques et un point de compétence à dépenser. Appuyez sur {menu} pour ouvrir votre "
		"écran de personnage ; je vous guiderai à l'intérieur.",
		"Vous avez gagné un niveau : vous avez des points de caractéristiques et un point de compétence à dépenser. Appuyez sur C pour ouvrir votre "
		"écran de personnage ; je vous guiderai à l'intérieur."},
	.levelUpSheet = {
		"Votre écran de personnage. Vous êtes sur le plus de la Force : {dpad} haut et bas parcourent les quatre caractéristiques, et "
		"maintenir {a} dépense un point sur celle où vous êtes. {rs} haut et bas lisent vos statistiques ligne par ligne, et "
		"droite dit les points qu'il vous reste.",
		"Votre écran de personnage. Les touches fléchées amènent le curseur sur le plus de chaque caractéristique, et A y dépense un point. "
		"J lit la description de la ligne où vous êtes, et Maj avec la flèche haut ou bas la parcourt ligne par "
		"ligne."},
	.levelUpSwitch = {
		"Vos points de caractéristiques sont dépensés. Maintenant votre point de compétence : appuyez sur {rb} jusqu'à atteindre l'arbre de compétences ; chaque menu "
		"dit son nom.",
		"Vos points de caractéristiques sont dépensés. Maintenant votre point de compétence : appuyez sur T pour ouvrir l'arbre de compétences à côté de votre écran de personnage."},
	.levelUpTree = {
		"L'arbre de compétences. LT et RT changent de branche, {dpad} parcourt ses compétences, et {rs} vers le haut lit celle où vous "
		"êtes. {a} y dépense le point, et {rs} vers la droite dit les points qu'il vous reste. Quand vous avez fini, fermez-le "
		"avec {b}.",
		"L'arbre de compétences. Page précédente et Page suivante changent de branche, les touches fléchées parcourent ses compétences, et J lit "
		"celle où se trouve le curseur. A y dépense le point, et Ctrl plus Page suivante dit les points qu'il vous "
		"reste. Quand vous avez fini, fermez-le avec Échap."},
	.levelUpTreeLater = {
		"Vous avez encore votre point de compétence. Appuyez sur {menu} : une fois les caractéristiques dépensées, il ouvre l'arbre de compétences.",
		"Vous avez encore votre point de compétence. Appuyez sur T pour ouvrir l'arbre de compétences."},
	.levelUpBind = {
		"Pour utiliser une compétence, placez-la sur un bouton : dans l'arbre de compétences, sur la compétence, appuyez sur {x} puis sur le bouton "
		"voulu. La lecture de chaque compétence dit sur quel bouton elle se trouve.",
		"Pour utiliser une compétence, placez-la dans une main : S ouvre la liste des compétences pour la main droite, et Maj plus S celle "
		"pour la main gauche ; les touches fléchées en choisissent une et A la place dans cette main. Une liste ouverte, F1 à F8 associent la compétence "
		"sous le curseur à cette touche."},
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
		"Terminé. Chaque fois que vous gagnez un niveau, {menu} ouvre votre écran de personnage. Retour à ce que vous faisiez :",
		"Terminé. Chaque fois que vous gagnez un niveau, C ouvre votre écran de personnage et T l'arbre de compétences. Retour à ce que vous "
		"faisiez :"},
	.routeStick = "{view} et {rs} vers le bas",
	.routeButton = "{view} et {a}",
	.firstMessage = "Premier message",
	.lastMessage = "Dernier message",
	.den = "Le Repaire du Mal",
	.moor = "La Lande sanglante",
};
}
