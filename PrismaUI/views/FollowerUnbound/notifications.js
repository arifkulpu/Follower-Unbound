(() => {
  const host = document.getElementById('fu-notifications');
  const maxVisible = 2;

  const removeToast = (el) => {
    if (!el) return;
    try {
      if (el.parentNode) {
        el.parentNode.removeChild(el);
      } else if (typeof el.remove === 'function') {
        el.remove();
      }
    } catch (_) {}
  };

  const iconFor = (message) => {
    const text = String(message || '').toLocaleLowerCase();
    if (/looted|loot|yağma|yagmal/.test(text)) return ['icon-loot-hand.png', 'Loot'];
    if (/craft|üret|ureti|bolt|arrow|torch|meşale|mesale/.test(text)) {
      if (/potion|potions|iksir/.test(text)) return ['icon-potion.png', 'Crafting'];
      if (/torch|meşale|mesale/.test(text)) return ['icon-lantern.png', 'Crafting'];
      if (/arrow|bolt|ok |ok\.|oklar|civata/.test(text)) return ['icon-arrows.png', 'Crafting'];
      return ['icon-anvil.png', 'Crafting'];
    }
    if (/purchase|purchased|buy |bought|sold|sale|satın|satin|satıldı|satildi|merchant|vendor|stock/.test(text)) {
      if (/sold|sale|satıldı|satildi/.test(text)) return ['icon-sell.png', 'Trade'];
      return ['icon-coin.png', 'Trade'];
    }
    if (/poison|zehir/.test(text)) return ['icon-poison.png', 'Poison'];
    if (/potion|cure|iksir|healed|tedavi/.test(text)) return ['icon-potion.png', 'Potions'];
    if (/thirst|drink|içecek|icecek|susuz/.test(text)) return ['icon-water.png', 'Needs'];
    if (/hunger|food|eat|yemek|hungry|aç |ac /.test(text)) return ['icon-food.png', 'Needs'];
    if (/collect|harvest|flower|ingredient|çiçek|cicek|topla/.test(text)) return ['icon-leaf.png', 'Gathering'];
    if (/equip|equipped|kuşan|kusan|armor|weapon|protection|protected|koruma|kıyafet|kiyafet/.test(text)) {
      return ['icon-shield.png', 'Equipment'];
    }
    if (/recover/.test(text)) return ['icon-loot-hand.png', 'Recovery'];
    if (/gold|coin|para/.test(text)) return ['icon-coin.png', 'Gold'];
    return ['icon-info.png', 'Follower Unbound'];
  };

  window.fuClearNotifications = () => {
    try {
      if (host) host.innerHTML = '';
    } catch (_) {}
  };

  window.fuNotify = (message) => {
    try {
      if (!host || typeof message !== 'string' || !message.trim()) return;

      const toast = document.createElement('div');
      toast.className = 'fu-toast';
      const [iconName, iconLabel] = iconFor(message);

      const icon = document.createElement('img');
      icon.className = 'fu-toast-icon';
      icon.src = `assets/icons/${iconName}`;
      icon.alt = '';
      icon.setAttribute('aria-hidden', 'true');
      icon.onerror = () => { icon.style.display = 'none'; };

      const text = document.createElement('div');
      text.className = 'fu-toast-text';
      text.textContent = message;

      toast.title = iconLabel;
      toast.appendChild(icon);
      toast.appendChild(text);
      host.appendChild(toast);

      // Sınırı aşan en eski bildirimleri güvenle kaldır
      while (host.children.length > maxVisible) {
        removeToast(host.firstElementChild);
      }

      window.setTimeout(() => {
        try {
          if (!toast.parentNode) return;
          toast.classList.add('fu-leave');
          window.setTimeout(() => removeToast(toast), 280);
        } catch (_) {}
      }, 4200);
    } catch (e) {
      console.error('fuNotify error:', e);
    }
  };
})();
