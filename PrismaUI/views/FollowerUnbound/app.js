(() => {
  const $ = (id) => document.getElementById(id);
  const send = (payload) => {
    if (typeof window.fuAction === 'function') window.fuAction(JSON.stringify(payload));
  };
  let state = { followers: [], settings: {} };
  let page = 'overview';
  let selectedFollower = 0;
  let gearTab = 'apparel';
  let economyTab = 'buy';
  let buyCategory = 'overview';
  let pickerOpen = false;
  let selectedGear = new Set();
  const pageTitles = { overview: 'Overview', equipment: 'Equipment', loot: 'Loot rules', economy: 'Trade & supplies', settings: 'Systems' };
  const esc = (v) => String(v ?? '').replace(/[&<>"']/g, (c) => ({ '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;' }[c]));
  const pct = (v) => Math.max(0, Math.min(100, Number(v) || 0));
  const follower = () => state.followers.find((f) => Number(f.id) === Number(selectedFollower)) || state.followers[0] || null;
  const money = (n) => Number(n || 0).toLocaleString();
  const notify = (message) => { const t = document.createElement('div'); t.className = 'toast'; t.textContent = message; document.body.append(t); setTimeout(() => t.remove(), 2450); };
  window.fuShowEquipPrompt = (prompt) => {
    const purchase = prompt?.mode === 'purchase';
    const lootUpgrade = prompt?.mode === 'loot' && Boolean(prompt?.currentStats || prompt?.candidateStats);
    if ($('equip-prompt-slot')) {
      const slot = prompt?.slotLabel;
      $('equip-prompt-slot').textContent = slot ? slot.toUpperCase() : '';
      $('equip-prompt-slot').hidden = !slot;
    }
    $('equip-prompt-title').textContent = purchase ? 'Equipment upgrade available' : (lootUpgrade ? 'Corpse loot equipment offer' : 'A better item was found');
    $('equip-prompt-summary').textContent = purchase
      ? `${prompt?.followerName || 'Your follower'} can buy ${prompt?.itemName || 'this item'} from a nearby seller.`
      : (lootUpgrade
        ? `${prompt?.followerName || 'Your follower'} will receive ${prompt?.itemName || 'this item'} from the loot.`
        : `${prompt?.followerName || 'Your follower'} found ${prompt?.itemName || 'a better item'}.`);
    $('equip-prompt-reason').textContent = purchase
      ? 'This compatible upgrade is available from a nearby seller.'
      : (lootUpgrade ? 'Compare the follower’s current gear with the item recovered from the corpse.' : (prompt?.reason || ''));
    $('equip-prompt-vendor').hidden = !purchase;
    $('equip-prompt-vendor').textContent = purchase ? `Seller: ${prompt.vendorName || 'Nearby merchant'}` : '';
    $('equipment-offer-comparison').hidden = !(purchase || lootUpgrade);
    $('equipment-offer-comparison').classList.toggle('loot-upgrade', lootUpgrade);
    $('equip-prompt-price-row').hidden = !purchase;
    $('equip-prompt-funds-row').hidden = !purchase;
    $('equipment-offer-comparison').querySelector('div:first-child span').textContent = lootUpgrade ? 'Currently equipped' : 'Current equipment';
    $('equipment-offer-comparison').querySelector('div:nth-child(2) span').textContent = purchase ? 'Purchase offer' : (lootUpgrade ? 'Recovered from corpse' : 'Loot item');
    const renderGearStats = (id, value) => {
      const parts = String(value || 'None').split(' · ');
      const name = parts.shift() || 'Unknown item';
      $(id).innerHTML = `<span class="gear-stat-name">${iconImg(iconFor(name), 'inline-icon')} ${esc(name)}</span>${parts.length ? `<span class="gear-stat-details">${esc(parts.join(' · '))}</span>` : ''}`;
    };
    renderGearStats('equip-prompt-current-stats', prompt?.currentStats);
    renderGearStats('equip-prompt-candidate-stats', prompt?.candidateStats);
    $('equip-prompt-price').textContent = purchase ? `${money(prompt.price)} gold` : '';
    $('equip-prompt-funds').textContent = purchase
      ? `${prompt?.followerName || 'Follower'}: ${money(prompt.spendableGold)} gold available${Number(prompt.spendableGold || 0) < Number(prompt.price || 0) ? ' — not enough' : ''}`
      : '';
    $('equip-prompt-copy').textContent = purchase
      ? 'Buy using this follower’s own spendable gold? The new item will be equipped and protected. Their current item will be unequipped, unprotected and available for sale.'
      : (lootUpgrade
        ? 'Give this item to the named follower, equip it and protect it? Their previous item will be unprotected and can be sold.'
        : 'Equip and protect this item? The previous item may become available for sale.');
    $('equip-prompt-no').textContent = purchase ? 'NO — NEVER ASK FOR THIS ITEM AGAIN' : 'KEEP CURRENT';
    $('equip-prompt-yes').textContent = purchase ? 'BUY, EQUIP & PROTECT' : 'EQUIP & PROTECT';
    $('equip-prompt').hidden = false;
  };
  window.fuHideEquipPrompt = () => { $('equip-prompt').hidden = true; };
  const resolveEquipPrompt = (accept) => {
    window.fuHideEquipPrompt();
    send({kind:'equipPromptDecision',accept});
  };
  $('equip-prompt-yes').addEventListener('click', () => resolveEquipPrompt(true));
  $('equip-prompt-no').addEventListener('click', () => resolveEquipPrompt(false));
  const iconFor = (value='') => {
    const s=String(value).toLowerCase();
    if(/poison/.test(s))return 'icon-poison';
    if(/invisibility|invisible/.test(s))return 'icon-invisibility';
    if(/magicka/.test(s))return 'icon-magicka';
    if(/stamina/.test(s))return 'icon-stamina';
    if(/health|potion|cure|alchemy/.test(s))return 'icon-health';
    if(/necklace|amulet/.test(s))return 'icon-necklace';
    if(/circlet/.test(s))return 'icon-circlet';
    if(/boot/.test(s))return 'icon-boots';
    if(/glove/.test(s))return 'icon-gloves';
    if(/helmet|helm/.test(s))return 'icon-helmet';
    if(/oil/.test(s))return 'icon-oil';
    if(/soap/.test(s))return 'icon-soap';
    if(/lantern/.test(s))return 'icon-lantern';
    if(/upgrade|better gear/.test(s))return 'icon-upgrade';
    if(/weapon|sword|dagger|axe|mace|warhammer|combat/.test(s))return 'icon-combat';
    if(/bow|bolt|arrow|ammo/.test(s))return 'icon-arrows';
    if(/armor|armour|clothing|robe|apparel|shield|outfit/.test(s))return /shield/.test(s)?'icon-shield':'icon-armor';
    if(/gold|coin|saving/.test(s))return 'icon-coin';
    if(/jewel|gem|valuable|soul|ring/.test(s))return 'icon-ring';
    if(/food|meat|bread|fruit|vegetable|stew|hunger/.test(s))return 'icon-food';
    if(/drink|water|milk|alcohol|thirst/.test(s))return 'icon-water';
    if(/book|scroll/.test(s))return 'icon-book';
    if(/ingredient|flower|plant/.test(s))return 'icon-leaf';
    if(/torch|fire/.test(s))return 'icon-fire';
    if(/craft|forge|anvil/.test(s))return 'icon-anvil';
    if(/sleep|fatigue|rest/.test(s))return 'icon-sleep';
    if(/lock|protect/.test(s))return 'icon-protected';
    if(/loot|corpse/.test(s))return 'icon-skull';
    if(/shop|trade|buy|sell|merchant/.test(s))return /sell/.test(s)?'icon-sell':'icon-cart';
    if(/bag|supply|utility|container/.test(s))return 'icon-sack';
    if(/notification|notify|alert|bell|message/.test(s))return 'icon-info';
    if(/follower|companion|party/.test(s))return 'icon-followers';
    if(/settings|system|gear/.test(s))return 'icon-settings';
    return 'icon-person';
  };
  const iconImg = (name,cls='') => `<img class="${cls}" src="assets/icons/${name}.png" alt="" aria-hidden="true">`;
  const itemLabel = (label,key=label) => `${iconImg(iconFor(`${key} ${label}`),'item-icon')}<span>${label}</span>`;
  const currentFollower = (id=selectedFollower) => state.followers.find(f=>Number(f.id)===Number(id)) || state.followers[0] || null;
  const followerPicker = (selected) => { const f=currentFollower(selected); const race=f?.portraitRace||'nord'; const portrait=f&&!f.animal?`<img class="picker-portrait" src="assets/portraits/${race}_${f.portraitFemale?'female':'male'}.jpg" alt="" onerror="this.classList.add('portrait-missing');this.removeAttribute('src')">`:'<span class="picker-portrait picker-placeholder">♟</span>'; return `<div class="follower-picker-wrap"><span>SELECT COMPANION</span><button type="button" class="follower-picker-control follower-picker-trigger" data-picker-toggle aria-expanded="${pickerOpen}">${portrait}<span class="follower-picker-name">${esc(f?.name||'No companion')}</span><span class="picker-chevron">${pickerOpen?'▲':'▼'}</span></button>${pickerOpen?`<div class="follower-picker-menu" role="listbox" aria-label="Select companion">${state.followers.map(option=>{const optionRace=option.portraitRace||'nord';const optionPortrait=!option.animal?`<img class="picker-option-portrait" src="assets/portraits/${optionRace}_${option.portraitFemale?'female':'male'}.jpg" alt="" onerror="this.classList.add('portrait-missing');this.removeAttribute('src')">`:'<span class="picker-option-portrait picker-placeholder">♟</span>';return `<button type="button" class="picker-option ${Number(option.id)===Number(selected)?'active':''}" role="option" aria-selected="${Number(option.id)===Number(selected)}" data-picker-option="${option.id}">${optionPortrait}<span><b>${esc(option.name)}</b><small>${esc(option.role||(option.animal?'Animal companion':'Companion'))}</small></span></button>`}).join('')}</div>`:''}</div>`; };
  const stat = (label, value, hint) => `<div class="stat-card"><div class="stat-label">${label}</div><div class="stat-value">${value}</div><div class="stat-hint">${hint}</div></div>`;
  const bar = (label, value, type='') => `<div><div class="bar-label"><span>${label}</span><span>${Math.round(pct(value))}%</span></div><div class="bar ${type}"><i style="width:${pct(value)}%"></i></div></div>`;
  const check = (title, key, obj, copy='') => { const values=obj==='settings'?state.settings:(follower()?.prefs||{}); return `<label class="toggle-row"><span class="toggle-copy-wrap"><span class="toggle-title">${itemLabel(title,key)}</span>${copy ? `<span class="toggle-copy">${copy}</span>` : ''}</span><span class="toggle"><input type="checkbox" data-${obj === 'settings' ? 'setting' : 'pref'}="${key}" ${values?.[key] ? 'checked' : ''}></span></label>`; };
  const globalCheck = (title,key,copy='') => `<label class="toggle-row"><span class="toggle-copy-wrap"><span class="toggle-title">${itemLabel(title,key)}</span>${copy?`<span class="toggle-copy">${copy}</span>`:''}</span><span class="toggle"><input type="checkbox" data-setting="${key}" ${state.settings?.[key]?'checked':''}></span></label>`;
  const renderOverview = () => {
    const humanoids=state.followers.filter(f=>!f.animal), avgNeeds=(key)=>state.followers.length?Math.round(state.followers.reduce((n,f)=>n+Number(f[key]||0),0)/state.followers.length):0;
    const avgHunger=avgNeeds('hunger'), avgThirst=avgNeeds('thirst'), avgFatigue=avgNeeds('fatigue');
    const totalGold = humanoids.reduce((n,f)=>n+Number(f.gold||0),0);
    const totalSpendable = humanoids.reduce((n,f)=>n+Number(f.spendableGold!=null?f.spendableGold:f.gold||0),0);
    const totalSaved = humanoids.reduce((n,f)=>n+Number(f.savedGold||0),0);
    $('content').innerHTML=`<div class="hero"><div><span class="eyebrow">THE PEOPLE AT YOUR SIDE</span><h1>Companions, in one place.</h1><p>Review their condition, equipment and daily routines.</p></div><button class="button secondary" data-refresh>SCAN PARTY</button></div>
      <div class="stats">${stat('Active companions',state.followers.length,'Followers and animal allies')}${stat('Party gold',money(totalGold),`${money(totalSpendable)} spendable${totalSaved>0?` · ${money(totalSaved)} saved`:''}`)}${stat('Spendable gold',money(totalSpendable),'Available for supplies & gear')}${stat('Average hunger',`${avgHunger}%`,'Across tracked companions')}${stat('Average thirst',`${avgThirst}%`,'Across tracked companions')}${stat('Average fatigue',`${avgFatigue}%`,'Across tracked companions')}</div>
      <div class="section-title">Your followers</div><div class="follower-grid">${state.followers.length?state.followers.map(f=>{const race=f.portraitRace||'nord';const portrait=!f.animal?`<img class="follower-portrait" src="assets/portraits/${race}_${f.portraitFemale?'female':'male'}.jpg" alt="${esc(race)} portrait" onerror="this.classList.add('portrait-missing');this.removeAttribute('src')">`:'';return `<article class="follower-card ${Number(f.id)===Number(selectedFollower)?'selected':''}" data-select-follower="${f.id}">${portrait}<div class="follower-card-body"><div class="follower-top"><div><div class="follower-name">${esc(f.name)}</div><div class="follower-sub">${esc(f.role|| (f.animal?'Animal companion':'Companion'))}${f.portraitRace?` · ${esc(f.portraitRace.replace('_',' '))} · ${f.portraitFemale?'Female':'Male'}`:''}</div></div><span class="role-pill">${f.animal?'ANIMAL':'FOLLOWER'}</span></div><div class="bars">${bar('Hunger',f.hunger)}${bar('Thirst',f.thirst,'thirst')}${bar('Fatigue',f.fatigue,'fatigue')}</div><div class="card-foot"><span class="follower-gold-foot" title="Spendable: ${money(f.spendableGold ?? f.gold)} | Saved: ${money(f.savedGold || 0)} | Total: ${money(f.gold)}">${iconImg(iconFor('gold'),'inline-icon')} <b>${money(f.spendableGold ?? f.gold)}</b> <small>spendable</small>${Number(f.savedGold || 0) > 0 ? ` <small class="gold-sub-detail">(${money(f.gold)} total)</small>` : ''}</span><span>Carry ${Math.round(f.carry||0)} / ${Math.round(f.capacity||0)}</span><span class="gold-actions"><button data-gold="100" data-follower="${f.id}">+100</button><button data-gold="500" data-follower="${f.id}">+500</button></span></div></div></article>`}).join(''):`<div class="empty">No active followers found. Press Scan Party to refresh.</div>`}</div>`;
  };
  const renderEquipment = () => {
    const f=follower(); if(!f){$('content').innerHTML='<div class="empty">No follower selected.</div>';return;}
    const rows=gearTab==='weapons'?(f.weapons||[]):(f.apparel||[]);
    $('content').innerHTML=`<div class="hero"><div><span class="eyebrow">LOADOUT CONTROL</span><h1>Equipment</h1><p>Protection and equipped status are separate. Protect items without selecting them to wear.</p></div>${followerPicker(f.id)}</div>
      <div class="panel equipment-panel"><div class="toolbar equipment-toolbar"><div><div class="section-title">${esc(f.name)}’s inventory</div><div class="notice">The EQUIPPED badge shows what they currently wear or wield. Protection keeps an item from sale; card selection plus the button equips it.</div></div><button class="button" data-equip-selected>WEAR & PROTECT SELECTED <span>(${selectedGear.size})</span></button></div>
      <div class="tabs"><button class="tab ${gearTab==='apparel'?'active':''}" data-gear-tab="apparel">APPAREL & ARMOR (${(f.apparel||[]).length})</button><button class="tab ${gearTab==='weapons'?'active':''}" data-gear-tab="weapons">WEAPONS (${(f.weapons||[]).length})</button></div>
      <div class="gear-list equipment-gear-list">${rows.length?rows.map(item=>`<div class="gear-card ${selectedGear.has(Number(item.id))?'chosen':''}" data-gear-card="${item.id}" role="button" tabindex="0" aria-pressed="${selectedGear.has(Number(item.id))}"><span class="gear-check" aria-hidden="true">${selectedGear.has(Number(item.id))?'✓':''}</span><span class="gear-name">${itemLabel(esc(item.name),gearTab==='weapons'?'weapon':'armor')}</span><span class="gear-meta">${gearTab==='weapons'?`DMG ${item.damage}`:`ARMOR ${item.armor}`}${item.count>1?` · ×${item.count}`:''}</span>${item.slot?`<span class="tag slot-tag">${esc(item.slot)}</span>`:''}${item.worn?'<span class="tag equipped-tag">EQUIPPED</span>':''}<label class="gear-state" data-protect-wrap><input type="checkbox" data-protect="${item.id}" ${item.protected?'checked':''}> PROTECTED</label></div>`).join(''):'<div class="empty">No items in this category.</div>'}</div></div>`;
  };
  const prefGroup = (title, keys, f) => `<div class="group-label">${title}</div><div class="toggle-grid">${keys.map(([key,label])=>check(label,key,f.prefs?'prefs':'prefs')).join('')}</div>`;
  const prefDefs = {
    loot:[['allowLoot','Allow looting'],['lootGold','Gold'],['lootWeapons','Weapons'],['lootArmor','Armor'],['lootClothing','Clothing & robes'],['lootPotions','Potions & poisons'],['lootFood','Food'],['lootDrinks','Drinks'],['lootArrows','Arrows & bolts'],['lootIngredients','Ingredients'],['lootSoulGems','Soul gems'],['lootGems','Gems & valuables'],['lootBooks','Books'],['lootScrolls','Scrolls']],
    economy:[]
  };
  const purchaseItems = [
    ['buyTreatmentPotions','maxTreatmentPotions','treatmentPotions','Cure Disease potion',25],
    ['buyCurePoisonPotions','maxCurePoisonPotions','curePoisonPotions','Cure Poison potion',25],
    ['buyFoodLarge','maxFoodLarge','foodLarge','Large food',25],
    ['buyFoodMedium','maxFoodMedium','foodMedium','Medium food',25],
    ['buyFoodSmall','maxFoodSmall','foodSmall','Small food',25],
    ['buyDrinkNormal','maxDrinkNormal','drinks','Water & milk',25],
    ['buyDrinkAlcohol','maxDrinkAlcohol','alcohol','Alcohol',25],
    ['buyArrows','maxArrows','arrows','Arrows',250],
    ['buyBolts','maxBolts','bolts','Crossbow bolts',250],
    ['buyPettySoulGems','maxPettySoulGems','pettySoulGems','Petty soul gems',20],
    ['buyLesserSoulGems','maxLesserSoulGems','lesserSoulGems','Lesser soul gems',20],
    ['buyCommonSoulGems','maxCommonSoulGems','commonSoulGems','Common soul gems',20],
    ['buyGreaterSoulGems','maxGreaterSoulGems','greaterSoulGems','Greater soul gems',20],
    ['buyGrandSoulGems','maxGrandSoulGems','grandSoulGems','Grand soul gems',20],
    ['buyBlackSoulGems','maxBlackSoulGems','blackSoulGems','Black soul gems',20],
    ['buyTorches','maxTorches','torches','Torches',20],
    ['buyLanternOil','maxLanternOil','lanternOil','Lantern oil',20],
    ['buySoap','maxSoap','soap','Soap',20],
    ['buyInvisibilityPotions','maxInvisibilityPotions','invisibilityPotions','Invisibility potions',25]
  ];
  const saleGroups = [
    ['WEAPONS & ARMOR',[['sellWeapons','Excess weapons'],['sellArmor','Excess armor'],['sellClothing','Clothing'],['sellJewelry','Jewelry']]],
    ['SUPPLIES',[['sellAmmo','Ammunition'],['sellPotions','Potions & poisons'],['sellFood','Food'],['sellDrinks','Drinks'],['sellIngredients','Ingredients']]],
    ['VALUABLES',[['sellSoulGems','Soul gems'],['sellGems','Gems & valuables'],['sellBooks','Books'],['sellScrolls','Scrolls']]]
  ];
  const renderEconomy = (f) => {
    const tabs=`<div class="tabs economy-tabs"><button class="tab ${economyTab==='buy'?'active':''}" data-economy-tab="buy">BUY SUPPLIES</button><button class="tab ${economyTab==='sell'?'active':''}" data-economy-tab="sell">SELL ITEMS</button><button class="tab ${economyTab==='craft'?'active':''}" data-economy-tab="craft">CRAFTING</button></div>`;
    const savings=`<section class="savings-panel"><div class="savings-heading">${iconImg('icon-coin','savings-icon')}<div><div class="section-title">Personal gold savings</div><p>Reserve part of new income. Set this follower’s target in Systems.</p></div><label class="toggle savings-switch"><input type="checkbox" data-pref="savingsEnabled" ${f.prefs.savingsEnabled?'checked':''} aria-label="Enable gold savings"></label></div><div class="savings-controls"><div class="savings-balance"><span>Saved</span><strong>${money(f.prefs.savedGold||0)} / ${money(f.prefs.savingsTarget||1000)} gold</strong></div><label><span>Income share</span><span class="percent-input"><input class="number-input" type="number" min="0" max="100" data-pref="savingsPercent" value="${Number(f.prefs.savingsPercent??30)}"><b>%</b></span></label></div></section>`;
    let panel='';
    if(economyTab==='buy'){
      const potionTiers=[
        ['HEALTH POTIONS','maxHealthPotions',[
          ['buyHealthMinor','healthMinor','Minor'],['buyHealthStandard','healthStandard','Standard'],['buyHealthPlentiful','healthPlentiful','Plentiful'],['buyHealthVigorous','healthVigorous','Vigorous'],['buyHealthExtreme','healthExtreme','Extreme'],['buyHealthUltimate','healthUltimate','Ultimate']]],
        ['MAGICKA POTIONS','maxMagickaPotions',[
          ['buyMagickaMinor','magickaMinor','Minor'],['buyMagickaStandard','magickaStandard','Standard'],['buyMagickaPlentiful','magickaPlentiful','Plentiful'],['buyMagickaVigorous','magickaVigorous','Vigorous'],['buyMagickaExtreme','magickaExtreme','Extreme'],['buyMagickaUltimate','magickaUltimate','Ultimate']]],
        ['STAMINA POTIONS','maxStaminaPotions',[
          ['buyStaminaMinor','staminaMinor','Minor'],['buyStaminaStandard','staminaStandard','Standard'],['buyStaminaPlentiful','staminaPlentiful','Plentiful'],['buyStaminaVigorous','staminaVigorous','Vigorous'],['buyStaminaExtreme','staminaExtreme','Extreme'],['buyStaminaUltimate','staminaUltimate','Ultimate']]]
      ];
      const potionSections=potionTiers.map(([title,targetKey,tiers])=>`<section class="purchase-category"><h3 class="group-label">${title}</h3>${tiers.map(([buyKey,stockKey,tier])=>`<div class="trade-row"><div class="trade-item"><span class="trade-name">${itemLabel(`${tier} ${title.split(' ')[0]} potion`,buyKey)}</span><span class="stock-badge">OWNED <b>${Number(f.stock?.[stockKey]||0)}</b></span></div><label class="trade-target"><span>Target</span><input class="number-input" type="number" min="0" max="25" data-pref="${targetKey}" value="${Number(f.prefs[targetKey]??5)}"></label><label class="toggle"><input type="checkbox" data-pref="${buyKey}" ${f.prefs[buyKey]?'checked':''} aria-label="Buy ${tier} ${title.toLowerCase()}"></label></div>`).join('')}</section>`).join('');
      const poisonTiers=[['buyPoisonHealth','poisonHealth','Health damage'],['buyPoisonMagicka','poisonMagicka','Magicka damage'],['buyPoisonStamina','poisonStamina','Stamina damage'],['buyPoisonOther','poisonOther','Other effects']];
      const poisonRows=poisonTiers.map(([buyKey,stockKey,label])=>`<div class="trade-row"><div class="trade-item"><span class="trade-name">${itemLabel(label+' poisons',buyKey)}</span><span class="stock-badge">OWNED <b>${Number(f.stock?.[stockKey]||0)}</b></span></div><label class="trade-target"><span>Target</span><input class="number-input" type="number" min="0" max="25" data-pref="maxPoisons" value="${Number(f.prefs.maxPoisons??5)}"></label><label class="toggle"><input type="checkbox" data-pref="${buyKey}" ${f.prefs[buyKey]?'checked':''} aria-label="Buy ${label} poisons"></label></div>`).join('');
      const poisonSection=`<section class="purchase-category"><h3 class="group-label">POISONS</h3><div class="sale-row"><span class="item-label">${itemLabel('Buy selected poison types','buyPoisons')}</span><label class="toggle"><input type="checkbox" data-pref="buyPoisons" ${f.prefs.buyPoisons?'checked':''} aria-label="Enable poison purchases"></label></div>${poisonRows}</section>`;
      const purchaseCategories=[
        ['treatment','Treatment potions','Cure disease and poison','icon-health',purchaseItems.slice(0,2)],
        ['food','Food & drink','Meals, water and beverages','icon-food',purchaseItems.slice(2,7)],
        ['ammo','Arrows & bolts','Ammunition supplies','icon-arrows',purchaseItems.slice(7,9)],
        ['soulgems','Soul gems','Six soul gem types','icon-ring',purchaseItems.slice(9,15)],
        ['utility','Travel & utility','Torches, oil and soap','icon-lantern',purchaseItems.slice(15,18)],
        ['special','Special potions','Invisibility potions','icon-invisibility',purchaseItems.slice(18)]
      ];
      const purchaseTooltips={
        buyFoodLarge:'Large food includes cooked meat, steak, chops, roasts, poultry and fish such as salmon. The item must also be flagged as food by the game.',
        buyFoodMedium:'Medium food includes bread, cheese, soups and stews, desserts, and other edible foods not classified as large or small.',
        buyFoodSmall:'Small food includes apples, carrots, cabbage, potatoes, leeks, tomatoes, berries, gourds, mushrooms, nuts and garlic.',
        buyDrinkNormal:'Non-alcoholic drinks include water, milk, juice, tea, cider and other drinks recognized by the mod. Waterskins are treated as non-alcoholic.',
        buyDrinkAlcohol:'Alcohol includes ale, mead, wine, brandy, beer, rum, vodka and liquor. Water or waterskins are excluded.'
      };
      const rowsFor=(items)=>items.map(([buyKey,maxKey,stockKey,label,max])=>{const tip=purchaseTooltips[buyKey];return `<div class="trade-row" ${tip?`data-tooltip="${esc(tip)}" aria-label="${esc(label)}. ${esc(tip)}"`:''}><div class="trade-item"><span class="trade-name">${itemLabel(label,buyKey||maxKey)}${tip?'<span class="info-hint" aria-hidden="true">i</span>':''}</span><span class="stock-badge">OWNED <b>${Number(f.stock?.[stockKey]||0)}</b></span></div><label class="trade-target"><span>Target stock</span><input class="number-input" type="number" min="${buyKey.includes('SoulGems')?'1':'0'}" max="${max}" data-pref="${maxKey}" value="${Number(f.prefs[maxKey]??0)}"></label><label class="toggle"><input type="checkbox" data-pref="${buyKey}" ${f.prefs[buyKey]?'checked':''} aria-label="Buy ${label}"></label></div>`;}).join('');
      const ingredients=[['blueMountainFlowers','Blue Mountain Flower'],['wheat','Wheat'],['redMountainFlowers','Red Mountain Flower'],['whiteCaps','White Cap'],['purpleMountainFlowers','Purple Mountain Flower'],['pineThrushEggs','Pine Thrush Egg']];
      const materialRows=ingredients.map(([key,label])=>`<div class="sale-row"><span class="item-label">${itemLabel(label,key)}</span><span class="stock-badge">OWNED <b>${Number(f.stock?.[key]||0)}</b></span></div>`).join('');
      const categoryContent={
        restorative:{title:'Health, magicka & stamina potions',icon:'icon-health',description:'Choose potion strengths and target stock.',html:potionSections},
        poisons:{title:'Poisons',icon:'icon-poison',description:'Choose poison effects and target stock.',html:poisonSection},
        ingredients:{title:'Potion ingredients',icon:'icon-leaf',description:'Bought from alchemists; harvested ingredients are shared.',html:`<section class="purchase-category ingredient-category"><p>Ingredients are bought from alchemists (wheat may also come from general merchants). Ingredients you harvest are shared among followers.</p>${materialRows}</section>`}
      };
      purchaseCategories.forEach(([id,title,description,icon,items])=>categoryContent[id]={title,icon,description,html:`<section class="purchase-category">${rowsFor(items)}</section>`});
      const buyContent=buyCategory==='overview'
        ? `<div class="buy-category-grid">${Object.entries(categoryContent).map(([id,c])=>`<button class="buy-category-card" data-buy-category="${id}">${iconImg(c.icon,'buy-category-icon')}<span class="buy-category-title">${c.title}</span><span class="buy-category-description">${c.description}</span><span class="buy-category-arrow">OPEN <b>→</b></span></button>`).join('')}</div>`
        : (()=>{const c=categoryContent[buyCategory]||categoryContent.restorative;return `<div class="buy-category-view"><div class="buy-category-view-head"><button class="button secondary" data-buy-back>← ALL CATEGORIES</button><div><span class="section-title">${iconImg(c.icon,'inline-icon')} ${c.title}</span><p>${c.description}</p></div></div><div class="trade-list buy-supplies-list">${c.html}</div></div>`;})();
      panel=`<section class="trade-panel"><div class="trade-panel-head"><div><div class="section-title">Purchase targets</div><p>Select a category to jump straight to its settings. Each enabled supply is stocked up to its target.</p></div><span class="trade-col-label">BUY</span></div>${buyContent}</section>`;
    } else if(economyTab==='sell'){
      panel=`<section class="trade-panel"><div class="trade-panel-head"><div><div class="section-title">Items to sell</div><p>Choose which categories followers may sell. Worn and protected equipment stays with them; supply limits from Purchases are kept in reserve.</p></div><span class="trade-col-label">SELL</span></div><div class="trade-list sale-grid">${saleGroups.map(([title,items])=>`<div class="sale-group"><div class="group-label">${itemLabel(title,title)}</div>${items.map(([key,label])=>`<div class="sale-row"><span class="item-label">${itemLabel(label,key)}</span><label class="toggle"><input type="checkbox" data-pref="${key}" ${f.prefs[key]?'checked':''} aria-label="${label}"></label></div>`).join('')}</div>`).join('')}</div></section>`;
    } else {
      const craftOptions=[
        ['allowCraftArrows','Craft arrows','Steel arrows: 1 Firewood + 1 Steel Ingot → 24 arrows. If no Steel Ingot is available: 1 Firewood + 1 Iron Ingot → 24 Iron Arrows.'],
        ['allowCraftBolts','Craft crossbow bolts','Steel Bolts: 1 Firewood + 1 Steel Ingot → 10 bolts.'],
        ['allowCraftTorches','Craft torches','1 Firewood + 1 Linen Wrap → 1 torch.'],
        ['allowCraftPotions','Craft potions','Healing: Blue Mountain Flower + Wheat. Magicka: Red Mountain Flower + White Cap. Stamina: Purple Mountain Flower + Pine Thrush Egg.']
      ];
      panel=`<section class="trade-panel"><div class="trade-panel-head"><div><div class="section-title">Crafting preferences</div><p>Hover over a crafting option to see its recipe. Arrows, bolts and torches require a forge within 400 game units. Potions require an alchemy lab within 400 units. Ingredients harvested by you are shared among followers.</p></div></div><div class="trade-list craft-list"><div class="trade-row"><div class="trade-item"><span class="trade-name">Ingredient stock cap</span><span class="stock-badge">PER INGREDIENT / FOLLOWER</span></div><label class="trade-target"><span>Maximum</span><input class="number-input" type="number" min="1" max="50" data-pref="maxPotionFlowers" value="${Number(f.prefs.maxPotionFlowers??30)}"></label></div>${craftOptions.map(([key,label,recipe])=>`<div class="sale-row craft-option" data-tooltip="${esc(recipe)}" aria-label="${esc(label)}. ${esc(recipe)}"><span class="item-label">${itemLabel(label,key)}</span><label class="toggle"><input type="checkbox" data-pref="${key}" ${f.prefs[key]?'checked':''} aria-label="${label}"></label></div>`).join('')}</div></section>`;
    }
    $('content').innerHTML=`<div class="hero"><div><span class="eyebrow">FOLLOWER ECONOMY</span><h1>Trade & supplies</h1><p>Configure savings, purchases, stock targets and sale categories for each companion.</p></div>${followerPicker(f.id)}</div><div class="panel economy-panel"><div class="section-title">${esc(f.name)}</div>${savings}${tabs}${panel}</div>`;
  };
  const renderPrefs = (which) => {
    const f=follower(); if(!f){$('content').innerHTML='<div class="empty">No follower selected.</div>';return;}
    if(which==='economy'){renderEconomy(f);return;}
    const title=which==='loot'?'Loot rules':'Trade & supplies';
    const keys=prefDefs[which];
    $('content').innerHTML=`<div class="hero"><div><span class="eyebrow">FOLLOWER PREFERENCES</span><h1>${title}</h1><p>Set what each companion collects, sells, buys and crafts.</p></div>${followerPicker(f.id)}</div><div class="panel"><div class="section-title">${esc(f.name)}</div><div class="toggle-grid">${keys.map(([key,label])=>check(label,key,'prefs')).join('')}</div>${which==='economy'?`<div class="group-label">STOCK LIMITS</div><div class="toggle-grid">${[['maxHealthPotions','Health potions'],['maxMagickaPotions','Magicka potions'],['maxStaminaPotions','Stamina potions'],['maxPoisons','Poisons'],['maxFoodLarge','Large food'],['maxFoodMedium','Medium food'],['maxFoodSmall','Small food'],['maxDrinkNormal','Water & milk'],['maxDrinkAlcohol','Alcohol'],['maxArrows','Arrows'],['maxBolts','Bolts'],['maxPettySoulGems','Petty soul gems'],['maxLesserSoulGems','Lesser soul gems'],['maxCommonSoulGems','Common soul gems'],['maxGreaterSoulGems','Greater soul gems'],['maxGrandSoulGems','Grand soul gems'],['maxBlackSoulGems','Black soul gems'],['maxTorches','Torches'],['maxLanternOil','Lantern oil'],['maxSoap','Soap']].map(([key,label])=>`<label class="toggle-row"><span class="toggle-title">${label}</span><input class="number-input" type="number" min="${key.includes('SoulGems')?'1':'0'}" max="${key.includes('SoulGems')?'20':'250'}" data-pref="${key}" value="${f.prefs[key]||0}"></label>`).join('')}</div>`:''}</div>`;
  };
  const renderSettings = () => {
    const f=follower();
    const groups=[
      ['CORE SYSTEMS',[
        ['autoLoot','Automatic loot','Share eligible leftovers after you close a container.'],
        ['autoEquip','Automatic equipment upgrades','Automatically equip eligible gear received through Follower Unbound.'],
        ['protectOutfit','Protect starting outfit','Capture equipped clothing when a follower is first registered.'],
        ['combatPotions','Combat potions','Use potions when combat health or magicka is low.'],
        ['needs','Needs system','Track hunger, thirst and fatigue.'],
        ['poison','Poison system','Apply poison to compatible weapons.'],
        ['shopping','Shopping','Buy supplies from nearby merchants.'],
        ['selling','Selling','Sell excess items to nearby merchants.'],
        ['promptBetterGear','Ask before equipping looted gear','Show a prompt before equipping a better item found in loot.'],
        ['equipmentPurchaseOffers','Offer better gear from merchants','Ask before a follower buys a better weapon or armor from a merchant.'],
        ['mageRobes','Mage robe preference','Prefer robes for mage-class followers.']
      ]],
      ['NOTIFICATIONS & ALERTS',[
        ['notifications','Enable all notifications','Master switch for all on-screen companion messages and alerts.'],
        ['notifyLoot','Looting alerts','Show messages when companions collect items or gold.'],
        ['notifyShopping','Shopping & stock alerts','Show messages for merchant purchases and low stock warnings.'],
        ['notifySelling','Trade & sales alerts','Show messages when excess items are sold to merchants.'],
        ['notifyNeeds','Needs alerts','Show messages when companions eat, drink or become starved.'],
        ['notifyCombatPotions','Combat potion & poison alerts','Show messages when potions or weapon poisons are used.'],
        ['notifyAutoEquip','Equipment upgrade alerts','Show messages when companions equip upgraded weapons or armor.']
      ]],
      ['GLOBAL LOOT CATEGORIES',[
        ['lootHostilesOnly','Hostile corpses only','Ignore neutral and friendly actors.'],
        ['lootGold','Gold'],
        ['lootWeapons','Weapons'],
        ['lootArmor','Armor'],
        ['lootArrows','Arrows & bolts'],
        ['lootPotions','Potions'],
        ['lootFood','Food'],
        ['lootIngredients','Ingredients'],
        ['lootSoulGems','Soul gems'],
        ['lootMisc','Other valuables']
      ]]
    ];
    $('content').innerHTML=`<div class="hero"><div><span class="eyebrow">GLOBAL BEHAVIOR</span><h1>Systems</h1><p>Control companion automation, notifications and global loot behavior.</p></div><div class="settings-actions">${followerPicker(f?.id)}<button class="button secondary" data-refresh>REFRESH STATE</button></div></div><div class="settings-columns">${groups.map(([title,items])=>`<section class="panel"><div class="group-label">${title}</div>${title.includes('NOTIFICATIONS')?`<div class="notification-actions"><button type="button" class="button secondary mini-btn" data-notif-bulk="true">ENABLE ALL</button><button type="button" class="button secondary mini-btn" data-notif-bulk="false">DISABLE ALL</button></div>`:''}<div class="toggle-grid">${items.map(([key,label,copy])=>globalCheck(label,key,copy)).join('')}</div></section>`).join('')}<section class="panel"><div class="group-label">EQUIPMENT UPGRADES</div><label class="toggle-row"><span><span class="toggle-title">Gear upgrade check chance (%)</span><span class="toggle-copy">Chance (1–100%) that followers look for or offer better weapons and armor.</span></span><input class="number-input" type="number" min="1" max="100" step="1" data-setting="equipmentUpgradeChance" value="${state.settings.equipmentUpgradeChance||35}"></label></section><section class="panel"><div class="group-label">FOLLOWER GOLD SAVINGS</div>${f?`<div class="savings-setting"><div><strong>${esc(f.name)}</strong><span>Saved ${money(f.prefs.savedGold||0)} of ${money(f.prefs.savingsTarget||1000)} gold</span></div><label><span>Target gold (1–10,000)</span><input class="number-input savings-target-input" type="number" min="1" max="10000" step="1" data-pref="savingsTarget" value="${Math.max(1,Math.min(10000,Number(f.prefs.savingsTarget??1000)))}"></label></div>`:'<div class="empty">No follower selected.</div>'}</section><section class="panel"><div class="group-label">LOOT TIMING</div><label class="toggle-row"><span><span class="toggle-title">Post-combat loot delay</span><span class="toggle-copy">Seconds before followers begin looting.</span></span><input class="number-input" type="number" min="0" max="60" step="1" data-setting="lootDelay" value="${state.settings.lootDelay||0}"></label></section></div>`;
  };
  const render = () => {
    if(!state.followers.some(f=>Number(f.id)===Number(selectedFollower))) selectedFollower=state.followers[0]?.id||0;
    $('page-title').textContent=pageTitles[page];
    $('follower-count').textContent=`${state.followers.length} companion${state.followers.length===1?'':'s'} tracked`;
    document.querySelectorAll('.nav-item').forEach(b=>b.classList.toggle('active',b.dataset.page===page));
    if(page==='overview')renderOverview();else if(page==='equipment')renderEquipment();else if(page==='loot'||page==='economy')renderPrefs(page);else renderSettings();
  };
  window.fuSetState=(next)=>{state={...state,...next};if(!selectedFollower&&state.followers.length)selectedFollower=state.followers[0].id;render();};
  $('navigation').addEventListener('click',e=>{const b=e.target.closest('[data-page]');if(!b)return;page=b.dataset.page;pickerOpen=false;render();});
  const toggleGearCard=(gear)=>{const id=Number(gear.dataset.gearCard);if(selectedGear.has(id))selectedGear.delete(id);else selectedGear.add(id);render();};
  $('content').addEventListener('change',e=>{
    const p=e.target.closest('[data-pref]'); if(p){const val=p.type==='checkbox'?p.checked:Number(p.value);send({kind:'preference',follower:Number(follower()?.id),key:p.dataset.pref,value:val});return;}
    const s=e.target.closest('[data-setting]');if(s){send({kind:'setting',key:s.dataset.setting,value:s.type==='checkbox'?s.checked:Number(s.value)});return;}
    const protect=e.target.closest('[data-protect]');if(protect){send({kind:'protect',follower:Number(follower()?.id),item:Number(protect.dataset.protect),value:protect.checked});return;}
  });
  $('content').addEventListener('click',e=>{
    const notifBulk=e.target.closest('[data-notif-bulk]');if(notifBulk){const enable=notifBulk.dataset.notifBulk==='true';send({kind:'notificationsBulk',enable});notify(enable?'All notifications enabled.':'All notifications disabled.');return;}
    const pickerToggle=e.target.closest('[data-picker-toggle]');if(pickerToggle){pickerOpen=!pickerOpen;render();return;}
    const pickerOption=e.target.closest('[data-picker-option]');if(pickerOption){selectedFollower=Number(pickerOption.dataset.pickerOption);selectedGear.clear();pickerOpen=false;render();return;}
    const gear=e.target.closest('[data-gear-card]');if(gear){if(e.target.closest('[data-protect-wrap]'))return;toggleGearCard(gear);return;}
    const gold=e.target.closest('[data-gold]');if(gold){e.stopPropagation();send({kind:'gold',follower:Number(gold.dataset.follower),amount:Number(gold.dataset.gold)});notify(`${gold.dataset.gold} gold given.`);return;}
    const pick=e.target.closest('[data-select-follower]');if(pick){selectedFollower=Number(pick.dataset.selectFollower);selectedGear.clear();render();return;}
    const tab=e.target.closest('[data-gear-tab]');if(tab){gearTab=tab.dataset.gearTab;selectedGear.clear();render();return;}
    const economy=e.target.closest('[data-economy-tab]');if(economy){economyTab=economy.dataset.economyTab;if(economyTab==='buy')buyCategory='overview';render();return;}
    const buyCategoryButton=e.target.closest('[data-buy-category]');if(buyCategoryButton){buyCategory=buyCategoryButton.dataset.buyCategory;render();return;}
    if(e.target.closest('[data-buy-back]')){buyCategory='overview';render();return;}
    if(e.target.closest('[data-refresh]')){send({kind:'refresh'});notify('Party scan requested.');return;}
    if(e.target.closest('[data-equip-selected]')){if(!selectedGear.size){notify('Select one or more items first.');return;}send({kind:'equipSelected',follower:Number(follower()?.id),items:[...selectedGear]});selectedGear.clear();notify('Selected equipment applied.');return;}
  });
  document.addEventListener('pointerdown',e=>{if(!pickerOpen||e.target.closest('.follower-picker-wrap'))return;pickerOpen=false;document.querySelectorAll('.follower-picker-menu').forEach(menu=>menu.remove());document.querySelectorAll('[data-picker-toggle]').forEach(button=>button.setAttribute('aria-expanded','false'));});
  const recipeTooltip=document.createElement('div');recipeTooltip.id='fu-recipe-tooltip';recipeTooltip.className='recipe-tooltip';recipeTooltip.setAttribute('role','tooltip');document.body.append(recipeTooltip);
  $('content').addEventListener('pointerover',e=>{const option=e.target.closest('[data-tooltip]');if(!option)return;recipeTooltip.textContent=option.dataset.tooltip;recipeTooltip.classList.add('visible');const r=option.getBoundingClientRect();const left=Math.max(12,Math.min(r.left,window.innerWidth-432));const top=(r.bottom+recipeTooltip.offsetHeight+18>window.innerHeight)?Math.max(12,r.top-recipeTooltip.offsetHeight-8):r.bottom+8;recipeTooltip.style.left=`${left}px`;recipeTooltip.style.top=`${top}px`;});
  $('content').addEventListener('pointerout',e=>{const option=e.target.closest('[data-tooltip]');if(option&&!option.contains(e.relatedTarget))recipeTooltip.classList.remove('visible');});
  $('content').addEventListener('focusin',e=>{const option=e.target.closest('[data-tooltip]');if(!option)return;recipeTooltip.textContent=option.dataset.tooltip;recipeTooltip.classList.add('visible');const r=option.getBoundingClientRect();recipeTooltip.style.left=`${Math.max(12,Math.min(r.left,window.innerWidth-432))}px`;recipeTooltip.style.top=`${Math.min(r.bottom+8,window.innerHeight-120)}px`;});
  $('content').addEventListener('focusout',e=>{if(e.target.closest('[data-tooltip]'))recipeTooltip.classList.remove('visible');});
  $('content').addEventListener('keydown',e=>{const gear=e.target.closest('[data-gear-card]');if(gear&&!e.target.closest('[data-protect-wrap]')&&(e.key==='Enter'||e.key===' ')){e.preventDefault();toggleGearCard(gear);}});
  $('refresh').addEventListener('click',()=>send({kind:'refresh'}));
  $('close').addEventListener('click',()=>send({kind:'close'}));
  window.addEventListener('keydown',e=>{if(e.key==='Escape'){e.preventDefault();if(pickerOpen){pickerOpen=false;render();return;}send({kind:'close'});}});
  render();
})();
