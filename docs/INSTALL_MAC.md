# Installing TEAROUT ENGINE on a Mac (Ableton Live 11)

1. Unzip `TEAROUT_ENGINE_macOS.zip`.
2. Copy the plugins into your plug-in folders. In Finder, use *Go → Go to Folder…*:
   - `TEAROUT ENGINE.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - `TEAROUT ENGINE.component` → `~/Library/Audio/Plug-Ins/Components/`
3. **Let macOS open it.** These builds are signed ad hoc but not notarized by Apple, so macOS quarantines them. Open **Terminal** and paste:

   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"TEAROUT ENGINE.vst3"
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"TEAROUT ENGINE.component"
   ```
4. In Live 11: **Live → Preferences → Plug-Ins**. Turn on **Use Audio Units** and/or **Use VST3 Plug-In System Folders**, then press **Rescan**.
5. In Live's browser, open **Plug-Ins → TEAROUT → TEAROUT ENGINE** and drag it onto a **MIDI track**. It's an instrument.

`TEAROUT ENGINE.app` is a standalone version. Use it to try the generator without opening Live, and drag MIDI out of it into anything.

**Apple Silicon and Intel:** the build is universal, so it runs natively on both.
