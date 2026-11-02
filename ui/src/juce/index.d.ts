// Types minimaux pour la bibliothèque front-end de JUCE 8 (copiée depuis
// JUCE/modules/juce_gui_extra/native/javascript, licence AGPLv3 / JUCE).

export interface ListenerList {
  addListener(fn: (...args: unknown[]) => void): number;
  removeListener(id: number): void;
}

export interface SliderProperties {
  start: number;
  end: number;
  skew: number;
  name: string;
  label: string;
  numSteps: number;
  interval: number;
  parameterIndex: number;
}

export interface SliderState {
  properties: SliderProperties;
  valueChangedEvent: ListenerList;
  propertiesChangedEvent: ListenerList;
  setNormalisedValue(value: number): void;
  getNormalisedValue(): number;
  getScaledValue(): number;
  sliderDragStarted(): void;
  sliderDragEnded(): void;
}

export function getSliderState(name: string): SliderState;
export function getNativeFunction(name: string): (...args: unknown[]) => Promise<unknown>;
export function getBackendResourceAddress(path: string): string;

declare global {
  interface Window {
    __JUCE__: {
      backend: {
        addEventListener(eventId: string, fn: (payload: unknown) => void): [string, number];
        removeEventListener(token: [string, number]): void;
        emitEvent(eventId: string, payload: unknown): void;
      };
      initialisationData: Record<string, unknown[]>;
    };
  }
}
