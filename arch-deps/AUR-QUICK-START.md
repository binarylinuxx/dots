# AUR Publishing Quick Start

Too lazy to publish 7 packages manually? Here's the lazy way:

## One-Time Setup (5 minutes)

1. **Generate SSH key** (if you don't have one):
   ```bash
   ssh-keygen -t ed25519 -C "your@email.com"
   # Just press Enter for all prompts
   ```

2. **Copy your public key**:
   ```bash
   cat ~/.ssh/id_ed25519.pub
   ```

3. **Add to AUR**:
   - Go to: https://aur.archlinux.org/account/
   - Login/Register
   - Paste key in "SSH Public Key" section

4. **Test it works**:
   ```bash
   ssh -T aur@aur.archlinux.org
   # Should say "Hi username! You've successfully authenticated"
   ```

## Publishing (30 seconds)

### First Time Publishing

Run the interactive script (reviews each package):
```bash
cd arch-deps/
./publish-to-aur.sh
```

It will:
- ✓ Check SSH setup
- ✓ Clone/create AUR repos
- ✓ Copy PKGBUILDs
- ✓ Generate .SRCINFO
- ✓ Show changes
- ✓ Ask for confirmation
- ✓ Push to AUR

### Updates (Even Lazier)

When you update PKGBUILDs later:

```bash
cd arch-deps/
./quick-publish.sh
```

That's it! No questions, just pushes everything.

## What Gets Published

- `blxshell-complete` - Meta package (all components)
- `blxshell-shell` - Terminal & shell
- `blxshell-hyprland` - Window manager
- `blxshell-audio` - Audio stack
- `blxshell-fonts` - Font bundle
- `blxshell-font-bitcount` - Bitcount font
- `blxshell-font-googlesans` - Google Sans font

## Troubleshooting

**"Permission denied (publickey)"**
→ SSH key not added to AUR or wrong key

**"remote: error: insufficient permission for adding an object"**
→ You don't own this package name on AUR (pick different name)

**"PKGBUILD: No such file or directory"**
→ Run from `arch-deps/` directory

## View Your Packages

After publishing:
https://aur.archlinux.org/packages?K=blxshell&SeB=n

## Even Lazier?

Create an alias:
```bash
echo 'alias aur-push="cd ~/dots/arch-deps && ./quick-publish.sh"' >> ~/.bashrc
```

Then just:
```bash
aur-push
```

Done! ☕
