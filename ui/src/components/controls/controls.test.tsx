// @vitest-environment jsdom
import { describe, it, expect, vi, afterEach } from 'vitest';
import { Slider } from './Slider';
import { Select } from './Select';
import { Toggle } from './Toggle';
import { Button } from './Button';
import { change, click, field, renderPlain } from '../../testing/harness';

let unmount = () => {};
afterEach(() => unmount());
const mount = async (element: Parameters<typeof renderPlain>[0]) => {
  const mounted = await renderPlain(element);
  unmount = mounted.unmount;
  return mounted.host;
};

describe('Slider', () => {
  it('shows its value and bounds, and reports the new number', async () => {
    const onChange = vi.fn();
    const host = await mount(<Slider label="Level" value={0.25} min={0} max={1} step={0.05} onChange={onChange} />);
    const input = field(host, 'Level');
    expect(input.type).toBe('range');
    expect([input.value, input.min, input.max, input.step]).toEqual(['0.25', '0', '1', '0.05']);
    await change(input, '0.6');
    expect(onChange).toHaveBeenCalledWith(0.6);
  });

  it('is inert when disabled', async () => {
    const host = await mount(<Slider label="Level" value={1} min={0} max={2} disabled onChange={() => {}} />);
    expect(field(host, 'Level').disabled).toBe(true);
  });

  it('with a skew maps value to position and position back to value', async () => {
    const skew = { toPosition: (v: number) => Math.sqrt(v / 100), toValue: (p: number) => p * p * 100 };
    const onChange = vi.fn();
    const host = await mount(<Slider label="Cutoff" value={25} min={0} max={100} skew={skew} onChange={onChange} />);
    const input = field(host, 'Cutoff');
    expect(Number(input.value)).toBeCloseTo(0.5, 6);
    await change(input, '0.8');
    expect(onChange).toHaveBeenCalledTimes(1);
    expect(onChange.mock.calls[0][0]).toBeCloseTo(64, 6);
  });
});

describe('Select', () => {
  const options = [{ value: '0', label: 'C' }, { value: '1', label: 'C#' }, 'D'];

  it('offers every option (string or value/label) and reports the chosen value', async () => {
    const onChange = vi.fn();
    const host = await mount(<Select label="Root" value="1" options={options} onChange={onChange} />);
    const select = field<HTMLSelectElement>(host, 'Root');
    expect([...select.options].map((o) => [o.value, o.textContent])).toEqual([['0', 'C'], ['1', 'C#'], ['D', 'D']]);
    expect(select.value).toBe('1');
    await change(select, 'D');
    expect(onChange).toHaveBeenCalledWith('D');
  });

  it('can be disabled', async () => {
    const host = await mount(<Select label="Root" value="0" options={options} disabled onChange={() => {}} />);
    expect(field<HTMLSelectElement>(host, 'Root').disabled).toBe(true);
  });
});

describe('Toggle', () => {
  it('reflects checked and reports the flipped value on click', async () => {
    const onChange = vi.fn();
    const host = await mount(<Toggle label="FX" checked={true} onChange={onChange} />);
    const box = field(host, 'FX');
    expect(box.checked).toBe(true);
    await click(box);
    expect(onChange).toHaveBeenCalledWith(false);
  });

  it('does not fire when disabled', async () => {
    const onChange = vi.fn();
    const host = await mount(<Toggle label="FX" checked={false} disabled onChange={onChange} />);
    await click(field(host, 'FX'));
    expect(onChange).not.toHaveBeenCalled();
  });
});

describe('Button', () => {
  it('fires onClick', async () => {
    const onClick = vi.fn();
    const host = await mount(<Button onClick={onClick}>Generate</Button>);
    await click(host.querySelector('button')!);
    expect(onClick).toHaveBeenCalledTimes(1);
    expect(host.textContent).toBe('Generate');
  });

  it('does not fire when disabled', async () => {
    const onClick = vi.fn();
    const host = await mount(<Button disabled onClick={onClick}>Randomize</Button>);
    await click(host.querySelector('button')!);
    expect(onClick).not.toHaveBeenCalled();
  });
});
