# Discovery run — blind, controller (Xbox naming)

Purpose: generate a `[ui]` trace of the front-end menus with the plugin's panel/navigation
announcements as the only orientation. Nothing here needs sight. Every screen change is spoken
("X opened", "going to Y"); B always goes back. If you get lost, press B several times: you end up
on the main menu, and "Salir de Diablo II" is the last item of the ribbon.

Button sequences come from the extracted layouts (`game-data/index/SUMMARY.md`).

1. Start NVDA. Launch `D2RLoader.exe`. Expect "Diablo 2 accessibility loaded".
2. Logos: "LogoMenuPanel opened" — press A to skip until "TitleScreenPanel opened".
3. Title screen ("Pulsa A para empezar"): press A.
4. If "GammaCalibrationPanel opened": A (default button is "Guardar y cerrar").
5. Main menu ("MainMenuPanel opened"). Do NOT press A yet:
   - D-pad down ×5 slowly (one per second), then D-pad up ×5. This is the key test: we want to
     see whether moving focus emits any message. Then D-pad left/right ×3 each.
6. Ribbon order is: Opciones, Vídeos, Créditos, Salir de Diablo II. Focus starts on JUGAR.
   Press D-pad down once, then A → expect "SettingsPanel opened" (Opciones).
7. Inside settings: RB ×3 slowly (tabs Vídeo → Audio → Juego → Controles; each should announce a
   sub-panel such as "SoundOptionsPanel opened"), then LB ×3. D-pad down ×4, up ×4 inside a tab.
   Press B → expect settings closed / back on main menu.
8. Main menu: D-pad down ×3 (Créditos), A → "CreditsModal opened"; B to close.
9. Main menu: D-pad up until JUGAR (or press B twice first), then A → expect
   "going to CharacterSelect" / "CharacterSelectPanel opened".
10. Character select: D-pad down ×3, up ×3. Then A on "Crear nuevo" (first button) → expect
    "going to CharacterCreate". D-pad left ×3, right ×3 (class carousel). B → back to select.
11. If a character already exists: D-pad to it, A → game loads ("GameJoined"...). In game:
    - Menu/Start button → pause ("PauseLayout"?). B to close.
    - View/Back button, then D-pad up/down/left/right (D2R controller radial/mini menu). B.
    - Menu button again → D-pad to "Guardar y salir", A.
12. Back at character select: B → main menu. D-pad down ×4 → "Salir de Diablo II", A.

Then tell me; I read `d2rloader/logs/d2r-access.log` and `d2rloader.log`.
