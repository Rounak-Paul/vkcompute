// Font switcher for VKCompute docs
(function() {
  const FONTS = [
    // Monospace fonts
    { id: 'space-mono', name: 'Space Mono', category: 'Monospace' },
    { id: 'vt323', name: 'VT323 (Pixel)', category: 'Monospace' },
    { id: 'jetbrains', name: 'JetBrains Mono', category: 'Monospace' },
    { id: 'fira', name: 'Fira Code', category: 'Monospace' },
    { id: 'ibm-plex', name: 'IBM Plex Mono', category: 'Monospace' },
    { id: 'roboto-mono', name: 'Roboto Mono', category: 'Monospace' },
    { id: 'source-code', name: 'Source Code Pro', category: 'Monospace' },
    // Sans-serif fonts
    { id: 'roboto', name: 'Roboto', category: 'Sans-serif' },
    { id: 'inter', name: 'Inter', category: 'Sans-serif' },
    { id: 'open-sans', name: 'Open Sans', category: 'Sans-serif' },
    { id: 'source-sans', name: 'Source Sans', category: 'Sans-serif' },
    { id: 'nunito', name: 'Nunito', category: 'Sans-serif' },
    { id: 'lato', name: 'Lato', category: 'Sans-serif' }
  ];

  const STORAGE_KEY = 'vkcompute-font';
  const DEFAULT_FONT = 'space-mono';

  function getStoredFont() {
    return localStorage.getItem(STORAGE_KEY) || DEFAULT_FONT;
  }

  function setFont(fontId) {
    // Remove all font classes
    FONTS.forEach(f => document.body.classList.remove('font-' + f.id));
    // Add selected font class
    document.body.classList.add('font-' + fontId);
    // Store preference
    localStorage.setItem(STORAGE_KEY, fontId);
  }

  function createFontSwitcher() {
    const container = document.createElement('div');
    container.className = 'font-switcher';

    const label = document.createElement('label');
    label.textContent = 'Font';
    label.setAttribute('for', 'font-select');

    const select = document.createElement('select');
    select.id = 'font-select';
    select.title = 'Choose font';

    const currentFont = getStoredFont();

    // Group fonts by category
    const categories = {};
    FONTS.forEach(font => {
      if (!categories[font.category]) {
        categories[font.category] = [];
      }
      categories[font.category].push(font);
    });

    // Create optgroups
    Object.keys(categories).forEach(category => {
      const optgroup = document.createElement('optgroup');
      optgroup.label = category;
      categories[category].forEach(font => {
        const option = document.createElement('option');
        option.value = font.id;
        option.textContent = font.name;
        if (font.id === currentFont) {
          option.selected = true;
        }
        optgroup.appendChild(option);
      });
      select.appendChild(optgroup);
    });

    select.addEventListener('change', (e) => {
      setFont(e.target.value);
    });

    container.appendChild(label);
    container.appendChild(select);

    return container;
  }

  function init() {
    // Apply stored font immediately
    setFont(getStoredFont());

    // Wait for page to load, then add switcher to header
    const addSwitcher = () => {
      const header = document.querySelector('.md-header__inner');
      if (header) {
        // Insert before the search/repo icons
        const target = header.querySelector('.md-header__source') || header.lastChild;
        const switcher = createFontSwitcher();
        header.insertBefore(switcher, target);
      }
    };

    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', addSwitcher);
    } else {
      addSwitcher();
    }
  }

  init();
})();
