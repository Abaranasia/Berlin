// @vitest-environment jsdom
import { describe, it, expect, vi, afterEach } from 'vitest';
import { act, useState } from 'react';
import { Knob } from './Knob';
import { SKEWS } from '../../lib/skew';
import { LIMITS } from '../../bridge/limits';
import { change, field, renderPlain } from '../../testing/harness';

let unmount = () => {};
afterEach(() => unmount());
const mount = async (element: Parameters<typeof renderPlain>[0]) => {
  const mounted = await renderPlain(element);
  unmount = mounted.unmount;
  return mounted.host;
};

const face = (host: HTMLElement) => host.querySelector<SVGElement>('[data-knob-face]')!;
const fire = (el: Element, type: string, init: MouseEventInit = {}) =>
  act(async () => void el.dispatchEvent(new MouseEvent(type, { bubbles: true, cancelable: true, buttons: 1, ...init })));
const dragFrom = async (el: Element, startY: number, ...moves: number[]) => {
  await fire(el, 'pointerdown', { clientY: startY });
  for (const y of moves) await fire(el, 'pointermove', { clientY: y });
};

describe('Knob as a range control', () => {
  it('keeps the Slider contract: aria-label, bounds, value and the reported number', async () => {
    const onChange = vi.fn();
    const host = await mount(<Knob label="Sustain" value={0.25} min={0} max={1} step={0.05} onChange={onChange} />);
    const input = field(host, 'Sustain');
    expect(input.type).toBe('range');
    expect([input.value, input.min, input.max, input.step]).toEqual(['0.25', '0', '1', '0.05']);
    expect(host.textContent).toContain('Sustain');
    expect(host.querySelector('output')!.textContent).toBe('0.25');
    await change(input, '0.6');
    expect(onChange).toHaveBeenCalledWith(0.6);
  });

  it('draws a decorative face hidden from assistive technology, with the input as the control', async () => {
    const host = await mount(<Knob label="Sustain" value={0.5} min={0} max={1} step={0.01} onChange={() => {}} />);
    expect(face(host).getAttribute('aria-hidden')).toBe('true');
    expect(host.querySelectorAll('input')).toHaveLength(1);
  });

  it('is inert when disabled', async () => {
    const host = await mount(<Knob label="Mix" value={1} min={0} max={2} disabled onChange={() => {}} />);
    expect(field(host, 'Mix').disabled).toBe(true);
  });

  it('lets the keyboard act natively: ArrowRight is not intercepted and the next step is reported', async () => {
    const onChange = vi.fn();
    const host = await mount(<Knob label="Sustain" value={0.5} min={0} max={1} step={0.01} onChange={onChange} />);
    const input = field(host, 'Sustain');
    const arrow = new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true, cancelable: true });
    await act(async () => void input.dispatchEvent(arrow));
    expect(arrow.defaultPrevented).toBe(false); // jsdom has no native step, so the browser's step is applied below
    await change(input, '0.51');
    expect(onChange).toHaveBeenCalledWith(0.51);
  });
});

describe('Knob skew', () => {
  it.each([
    ['Cutoff', 1000, SKEWS.cutoffHz, LIMITS.cutoffHz],
    ['Resonance', 2, SKEWS.resonance, LIMITS.resonance],
    ['Attack', 0.2, SKEWS.attack, LIMITS.attackSeconds],
    ['Decay', 0.3, SKEWS.decay, LIMITS.decaySeconds],
    ['Release', 0.5, SKEWS.release, LIMITS.releaseSeconds],
    ['LFO rate', 2, SKEWS.lfoRateHz, LIMITS.lfoRateHz],
  ])('%s at its midpoint value sits at position 0.5 on a 0..1 "any" input and reports skewed values', async (label, midpoint, skew, limits) => {
    const onChange = vi.fn();
    const host = await mount(<Knob label={label} value={midpoint} min={limits.min} max={limits.max} skew={skew} onChange={onChange} />);
    const input = field(host, label);
    expect([input.min, input.max, input.step]).toEqual(['0', '1', 'any']);
    expect(Number(input.value)).toBeCloseTo(0.5, 6);
    await change(input, '0.7');
    expect(onChange.mock.calls[0][0]).toBeCloseTo(skew.toValue(0.7), 6);
  });
});

describe('Knob vertical drag', () => {
  const linear = (onChange: (v: number) => void, extra: { disabled?: boolean } = {}) => (
    <Knob label="Sustain" value={0.5} min={0} max={1} step={0.01} onChange={onChange} {...extra} />
  );

  it('200 px of travel is the full range, relative to where the drag started', async () => {
    const onChange = vi.fn();
    const host = await mount(linear(onChange));
    await dragFrom(face(host), 300, 250);
    expect(onChange).toHaveBeenLastCalledWith(0.75); // 50 px up from 0.5
    await fire(face(host), 'pointermove', { clientY: 350 });
    expect(onChange).toHaveBeenLastCalledWith(0.25); // 50 px down from the start, not from the last move
  });

  it('clamps at both ends of the range', async () => {
    const onChange = vi.fn();
    const host = await mount(linear(onChange));
    await dragFrom(face(host), 300, 100, 600);
    expect(onChange.mock.calls.map((c) => c[0])).toEqual([1, 0]);
  });

  it('reports only when the value changes', async () => {
    const onChange = vi.fn();
    const host = await mount(linear(onChange));
    await dragFrom(face(host), 300, 250, 250, 250.4);
    expect(onChange.mock.calls.map((c) => c[0])).toEqual([0.75]); // 250.4 snaps to the same 0.75
  });

  it('moves a skewed knob over its position range', async () => {
    const onChange = vi.fn();
    const host = await mount(<Knob label="Cutoff" value={1000} min={20} max={20000} skew={SKEWS.cutoffHz} onChange={onChange} />);
    await dragFrom(face(host), 300, 200);
    expect(onChange).toHaveBeenCalledTimes(1);
    expect(onChange.mock.calls[0][0]).toBeCloseTo(SKEWS.cutoffHz.toValue(1.0), 3); // 100 px up from the midpoint = the max
  });

  it('focuses its input on pointerdown so the keyboard continues from there', async () => {
    const host = await mount(linear(() => {}));
    await fire(face(host), 'pointerdown', { clientY: 300 });
    expect(document.activeElement).toBe(field(host, 'Sustain'));
  });

  it('stops tracking after pointerup and pointercancel, and ignores moves that never started', async () => {
    const onChange = vi.fn();
    const host = await mount(linear(onChange));
    await fire(face(host), 'pointermove', { clientY: 100 });
    expect(onChange).not.toHaveBeenCalled();
    await dragFrom(face(host), 300, 250);
    await fire(face(host), 'pointerup');
    await fire(face(host), 'pointermove', { clientY: 100 });
    await dragFrom(face(host), 300);
    await fire(face(host), 'pointercancel');
    await fire(face(host), 'pointermove', { clientY: 100 });
    expect(onChange.mock.calls.map((c) => c[0])).toEqual([0.75]);
  });

  it('dispatches nothing while disabled', async () => {
    const onChange = vi.fn();
    const host = await mount(linear(onChange, { disabled: true }));
    await dragFrom(face(host), 300, 100);
    expect(onChange).not.toHaveBeenCalled();
    expect(document.activeElement).not.toBe(field(host, 'Sustain'));
  });

  it('a button-less move ends the drag and dispatches nothing', async () => {
    const onChange = vi.fn();
    const host = await mount(linear(onChange));
    await dragFrom(face(host), 300);
    await fire(face(host), 'pointermove', { clientY: 250, buttons: 0 });
    await fire(face(host), 'pointermove', { clientY: 250, buttons: 1 }); // the drag already ended
    expect(onChange).not.toHaveBeenCalled();
  });

  it('lostpointercapture ends the drag', async () => {
    const onChange = vi.fn();
    const host = await mount(linear(onChange));
    await dragFrom(face(host), 300, 250);
    await fire(face(host), 'lostpointercapture');
    await fire(face(host), 'pointermove', { clientY: 100 });
    expect(onChange.mock.calls.map((c) => c[0])).toEqual([0.75]);
  });

  it('dispatches nothing once disabled flips mid-drag', async () => {
    const onChange = vi.fn();
    const Wrapper = () => {
      const [disabled, setDisabled] = useState(false);
      return (
        <Knob
          label="Sustain"
          value={0.5}
          min={0}
          max={1}
          step={0.01}
          disabled={disabled}
          onChange={(v) => {
            onChange(v);
            setDisabled(true);
          }}
        />
      );
    };
    const host = await mount(<Wrapper />);
    await dragFrom(face(host), 300, 250, 200, 150);
    expect(onChange.mock.calls.map((c) => c[0])).toEqual([0.75]);
  });
});

describe('Knob has no wheel or double-click behavior', () => {
  it('leaves the value alone on wheel and double-click, on the face and on the input', async () => {
    const onChange = vi.fn();
    const host = await mount(<Knob label="Sustain" value={0.5} min={0} max={1} step={0.01} onChange={onChange} />);
    const input = field(host, 'Sustain');
    for (const target of [face(host), input]) {
      await act(async () => void target.dispatchEvent(new WheelEvent('wheel', { bubbles: true, cancelable: true, deltaY: -100 })));
      await fire(target, 'dblclick');
    }
    expect(onChange).not.toHaveBeenCalled();
    expect(input.value).toBe('0.5');
  });
});
