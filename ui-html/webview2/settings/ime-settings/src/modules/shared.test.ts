import { afterEach, beforeEach, expect, it, vi } from 'vitest';
import { setupDropdownMenu } from './shared';

vi.mock('./theme', () => ({ setSurfaceTheme: vi.fn(), setThemeMode: vi.fn() }));

class MenuElement extends EventTarget {
  textContent = '';
  dataset: Record<string, string> = {};
  attributes = new Map<string, string>();
  classList = { remove: vi.fn() };
  label?: MenuElement;
  querySelector() { return this.label; }
  contains() { return true; }
  closest() { return this; }
  getAttribute(name: string) { return this.attributes.get(name) ?? null; }
}

let menu: MenuElement;
let item: MenuElement;
let label: MenuElement;
let postMessage: ReturnType<typeof vi.fn>;
beforeEach(() => {
  const button = new MenuElement();
  label = new MenuElement();
  label.textContent = '[ / ]';
  button.label = label;
  menu = new MenuElement();
  item = new MenuElement();
  item.textContent = '- / =';
  item.dataset.value = 'minus_equal';
  postMessage = vi.fn();
  vi.stubGlobal('HTMLInputElement', class {});
  vi.stubGlobal('window', { chrome: { webview: { postMessage } } });
  vi.stubGlobal('document', {
    getElementById: (id: string) => id === 'button' ? button : menu,
    addEventListener: vi.fn()
  });
  setupDropdownMenu('button', 'menu', '', true, 'input.word_to_character_keys');
});
afterEach(() => vi.unstubAllGlobals());

function clickItem() {
  const event = new Event('click');
  Object.defineProperty(event, 'target', { value: item });
  menu.dispatchEvent(event);
}

it('does not select or send a disabled dropdown item', () => {
  item.attributes.set('aria-disabled', 'true');
  clickItem();
  expect(label.textContent).toBe('[ / ]');
  expect(postMessage).not.toHaveBeenCalled();
  expect(menu.classList.remove).not.toHaveBeenCalled();
});

it('allows the item after the host clears its disabled state', () => {
  item.attributes.set('aria-disabled', 'true');
  clickItem();
  item.attributes.set('aria-disabled', 'false');
  clickItem();
  expect(label.textContent).toBe('- / =');
  expect(postMessage).toHaveBeenCalledTimes(1);
  const message = postMessage.mock.calls[0]?.[0];
  expect(typeof message === 'string' ? JSON.parse(message) : message).toMatchObject({
    type: 'configUpdate', data: { path: 'input.word_to_character_keys', value: 'minus_equal' }
  });
  expect(menu.classList.remove).toHaveBeenCalledWith('open');
});
