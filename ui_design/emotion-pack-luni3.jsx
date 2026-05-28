/* Luni — emotion + scene pack #3.
   Adds emotions: brave, dreamy, awe
   Adds scenes:   delivery, flashlight, podcast, stopwatch
   Loaded AFTER emotion-pack-luni2.jsx.
   Checked for overlap with: every existing category in cats.
*/

const {
  W, H, STATUS_H, CY, GAP, LX, RX, EYE_W, EYE_H, EYE_RX,
  TAU, lerp, clamp, wrap, ease, pulse, blink,
  Eye, Eyes, heartPath, starPath, arcPath,
} = window.EmotionCore;

const cats = window.EMOTION_CATEGORIES;

// =============================================================
// EMOTION: BRAVE — 3 variants
// =============================================================
cats.brave = {
  label: 'Brave',
  desc: 'Fierce / steady courage.',
  tone: 'red',
  variants: [
    { id: 'brave-stance', label: 'Fierce stance', duration: 2400,
      render: (t) => {
        const breath = Math.sin(t * TAU * 0.7) * 1.5;
        return (
          <g>
            {/* sharp, narrowed eyes */}
            <Eyes L={{ h: EYE_H * 0.5, rx: 8, y: CY + 4 + breath }}
                  R={{ h: EYE_H * 0.5, rx: 8, y: CY + 4 + breath }} />
            {/* strong V-brows */}
            <g stroke="var(--eye)" strokeWidth={9} strokeLinecap="round">
              <line x1={LX - 38} y1={CY - 44} x2={LX + 28} y2={CY - 26} />
              <line x1={RX + 38} y1={CY - 44} x2={RX - 28} y2={CY - 26} />
            </g>
            {/* small flame icon over center */}
            <g transform={`translate(${W / 2} ${STATUS_H + 24})`}>
              <path d={`M 0 8
                        C -8 4, -8 -6, -2 -10
                        C -2 -4, 4 -2, 4 -10
                        C 8 -6, 8 4, 0 8 Z`}
                    fill="var(--accent)"
                    transform={`scale(${1 + breath * 0.05})`} />
            </g>
          </g>
        );
      } },

    { id: 'brave-shield', label: 'Shield up', duration: 2200,
      render: (t) => {
        const pop = t < 0.2 ? ease.out(t / 0.2) : 1;
        return (
          <g>
            {/* shield centered, slightly behind eyes */}
            <g transform={`translate(${W / 2} ${CY + 8}) scale(${pop})`}>
              <path d="M 0 -42
                       L 40 -34
                       L 36 8
                       Q 30 28, 0 42
                       Q -30 28, -36 8
                       L -40 -34 Z"
                    fill="var(--eye)" />
              <path d="M -6 -22 L 6 -22 L 6 0 L 14 0 L 0 16 L -14 0 L -6 0 Z"
                    fill="var(--bg)" opacity={0.7} />
            </g>
            {/* eye peeks at the top corners of the shield */}
            <Eye x={LX} y={CY - 32} h={EYE_H * 0.35} rx={10} />
            <Eye x={RX} y={CY - 32} h={EYE_H * 0.35} rx={10} />
          </g>
        );
      } },

    { id: 'brave-charge', label: 'Charge', duration: 1200,
      render: (t) => {
        const step = Math.sin(t * TAU * 2) * 6;
        return (
          <g transform={`translate(${step} 0)`}>
            {/* angled determined eyes */}
            <g>
              <Eye x={LX} y={CY} h={EYE_H * 0.55} rx={10} rot={-8} />
              <Eye x={RX} y={CY} h={EYE_H * 0.55} rx={10} rot={8} />
            </g>
            {/* speed lines trailing behind */}
            {[0, 1, 2, 3].map((i) => {
              const p = ((t * 3) + i * 0.25) % 1;
              return (
                <line key={i}
                      x1={2 + p * 28} y1={STATUS_H + 30 + i * 36}
                      x2={20 + p * 28} y2={STATUS_H + 30 + i * 36}
                      stroke="var(--accent)" strokeWidth={2.5}
                      strokeLinecap="round" opacity={1 - p} />
              );
            })}
            {/* forward arrow */}
            <path d={`M ${W - 28} ${CY - 10}
                      L ${W - 12} ${CY}
                      L ${W - 28} ${CY + 10}
                      M ${W - 12} ${CY} L ${W - 40} ${CY}`}
                  fill="none" stroke="var(--accent)" strokeWidth={4}
                  strokeLinecap="round" strokeLinejoin="round" />
          </g>
        );
      } },
  ],
};

// =============================================================
// EMOTION: DREAMY — 3 variants
// =============================================================
cats.dreamy = {
  label: 'Dreamy',
  desc: 'Daydreaming / head in clouds.',
  tone: 'purple',
  variants: [
    { id: 'dreamy-clouds', label: 'Floating clouds', duration: 4400,
      render: (t) => {
        const drift = Math.sin(t * TAU * 0.4) * 4;
        return (
          <g>
            {/* soft, slightly upturned eyes */}
            <Eyes L={{ y: CY - 2 + drift, h: EYE_H * 0.75, rx: 28 }}
                  R={{ y: CY - 2 + drift, h: EYE_H * 0.75, rx: 28 }} />
            {/* small twinkle in each eye */}
            <circle cx={LX - 14} cy={CY - 8 + drift} r={5} fill="var(--bg)" opacity={0.85} />
            <circle cx={RX - 14} cy={CY - 8 + drift} r={5} fill="var(--bg)" opacity={0.85} />
            {/* clouds drifting across top */}
            {[0, 1, 2].map((i) => {
              const seed = i * 0.33;
              const p = ((t * 0.5) + seed) % 1;
              const x = -40 + p * (W + 80);
              const y = STATUS_H + 8 + (i % 2) * 6;
              return (
                <g key={i} transform={`translate(${x} ${y})`}
                   fill="var(--accent)" opacity={0.7}>
                  <ellipse cx={0} cy={0} rx={10} ry={5} />
                  <ellipse cx={8} cy={-3} rx={8} ry={5} />
                  <ellipse cx={-8} cy={-2} rx={7} ry={4} />
                </g>
              );
            })}
          </g>
        );
      } },

    { id: 'dreamy-bubbles', label: 'Thought bubbles', duration: 3800,
      render: (t) => {
        const lookUp = Math.sin(t * TAU * 0.5) * 4 - 8;
        return (
          <g>
            <Eyes L={{ y: CY + 4, h: EYE_H * 0.7 }}
                  R={{ y: CY + 4, h: EYE_H * 0.7 }} />
            {/* pupils look up-right */}
            <circle cx={LX + 8} cy={CY + 4 + lookUp} r={10} fill="var(--bg)" />
            <circle cx={RX + 8} cy={CY + 4 + lookUp} r={10} fill="var(--bg)" />
            {/* rising thought bubbles */}
            {[0, 1, 2, 3].map((i) => {
              const seed = i * 0.25;
              const p = ((t * 0.7) + seed) % 1;
              const x = RX + 30 + p * 24;
              const y = lerp(CY - 20, STATUS_H + 8, p);
              const r = lerp(3, 14, p);
              return (
                <circle key={i} cx={x} cy={y} r={r}
                        fill="none" stroke="var(--accent)" strokeWidth={2}
                        opacity={1 - p * 0.4} />
              );
            })}
          </g>
        );
      } },

    { id: 'dreamy-sparkle', label: 'Distant sparkle', duration: 3000,
      render: (t) => (
        <g>
          {/* unfocused soft eyes */}
          <Eyes L={{ h: EYE_H * 0.65, rx: 28 }}
                R={{ h: EYE_H * 0.65, rx: 28 }} />
          {/* hollow centers — gaze goes through */}
          <circle cx={LX} cy={CY} r={14} fill="var(--bg)" opacity={0.6} />
          <circle cx={RX} cy={CY} r={14} fill="var(--bg)" opacity={0.6} />
          {/* distant sparkle field */}
          {Array.from({ length: 7 }).map((_, i) => {
            const seed = ((i * 53) % 100) / 100;
            const p = ((t * 0.8) + seed) % 1;
            const x = (i * 47) % (W - 20) + 10;
            const y = STATUS_H + 14 + ((i * 31) % 60);
            const s = 2 + Math.abs(Math.sin(p * TAU)) * 3;
            return (
              <path key={i} d={starPath(x, y, s, s * 0.4)}
                    fill="var(--accent)" opacity={0.4 + Math.abs(Math.sin(p * TAU)) * 0.5} />
            );
          })}
        </g>
      ) },
  ],
};

// =============================================================
// EMOTION: AWE — 2 variants (distinct from surprised: sustained wonder,
//   not sudden shock)
// =============================================================
cats.awe = {
  label: 'Awe',
  desc: 'Wonder / quiet amazement.',
  tone: 'white',
  variants: [
    { id: 'awe-gaze', label: 'Slow gaze', duration: 3600,
      render: (t) => {
        const lift = (Math.sin(t * TAU - Math.PI / 2) + 1) / 2;
        const sz = 1 + lift * 0.15;
        return (
          <g>
            {/* wide round eyes that grow slowly */}
            <Eyes L={{ w: EYE_W * 1.08 * sz, h: EYE_H * 1.05 * sz }}
                  R={{ w: EYE_W * 1.08 * sz, h: EYE_H * 1.05 * sz }} />
            {/* dual highlights — twinkle stars inside */}
            <path d={starPath(LX - 12, CY - 12, 8, 3.2)}
                  fill="var(--bg)" opacity={0.85} />
            <path d={starPath(RX - 12, CY - 12, 8, 3.2)}
                  fill="var(--bg)" opacity={0.85} />
            {/* small upturned mouth (parted lips of wonder) */}
            <ellipse cx={W / 2} cy={H - 30}
                     rx={5 + lift * 3} ry={3 + lift * 5}
                     fill="none" stroke="var(--eye)" strokeWidth={3} />
          </g>
        );
      } },

    { id: 'awe-rays', label: 'Light rays', duration: 3200,
      render: (t) => (
        <g>
          {/* radial rays emanating from a high source */}
          {Array.from({ length: 9 }).map((_, i) => {
            const a = -Math.PI + (i / 8) * Math.PI; // -180 to 0 → top half
            const len = 70 + Math.sin(t * TAU + i) * 6;
            const x1 = W / 2 + Math.cos(a) * 12;
            const y1 = STATUS_H + 6 + Math.sin(a) * 12;
            const x2 = W / 2 + Math.cos(a) * len;
            const y2 = STATUS_H + 6 + Math.sin(a) * len;
            return (
              <line key={i} x1={x1} y1={y1} x2={x2} y2={y2}
                    stroke="var(--accent)" strokeWidth={2}
                    strokeLinecap="round" opacity={0.45} />
            );
          })}
          {/* lit-up wide eyes looking up */}
          <Eyes L={{ y: CY + 4, w: EYE_W, h: EYE_H * 0.95 }}
                R={{ y: CY + 4, w: EYE_W, h: EYE_H * 0.95 }} />
          {/* pupils tucked at top of eye */}
          <ellipse cx={LX} cy={CY - 16} rx={12} ry={8} fill="var(--bg)" />
          <ellipse cx={RX} cy={CY - 16} rx={12} ry={8} fill="var(--bg)" />
        </g>
      ) },
  ],
};

// =============================================================
// SCENE: DELIVERY — 3 variants
// =============================================================
cats.delivery = {
  label: 'Delivery',
  desc: 'Package · truck · doorstep.',
  kind: 'scene',
  tone: 'orange',
  variants: [
    { id: 'delivery-box', label: 'Box arrived', duration: 2400,
      render: (t) => {
        const drop = t < 0.4 ? lerp(-60, 0, ease.out(t / 0.4)) : 0;
        const squish = t > 0.4 && t < 0.55
          ? 1 + Math.sin(((t - 0.4) / 0.15) * Math.PI) * 0.08
          : 1;
        return (
          <g>
            {/* ground line */}
            <line x1={W / 2 - 80} y1={H - 24} x2={W / 2 + 80} y2={H - 24}
                  stroke="var(--eye)" strokeWidth={2} opacity={0.4} />
            {/* box */}
            <g transform={`translate(${W / 2} ${CY + drop}) scale(${1 / squish} ${squish})`}>
              <rect x={-40} y={-30} width={80} height={56} rx={2}
                    fill="var(--eye)" />
              {/* tape lines */}
              <line x1={0} y1={-30} x2={0} y2={26}
                    stroke="var(--accent)" strokeWidth={4} />
              <line x1={-40} y1={-2} x2={40} y2={-2}
                    stroke="var(--accent)" strokeWidth={4} />
              {/* arrow icon (this side up) */}
              <g transform="translate(-20 14)" stroke="var(--bg)"
                 strokeWidth={1.5} fill="none">
                <path d="M -6 4 L 0 -2 L 6 4 M 0 -2 L 0 6" strokeLinecap="round" />
              </g>
            </g>
            {/* "DELIVERED" text appears after drop */}
            {t > 0.55 && (
              <text x={W / 2} y={H - 8} textAnchor="middle"
                    fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={11} fontWeight={700}
                    fill="var(--accent)" letterSpacing={3}
                    opacity={clamp((t - 0.55) * 3, 0, 1)}>
                DELIVERED
              </text>
            )}
          </g>
        );
      } },

    { id: 'delivery-truck', label: 'Truck', duration: 2200,
      render: (t) => {
        const x = lerp(-100, W + 100, t);
        const wheelRot = (t * 1200) % 360;
        return (
          <g transform={`translate(${x} ${CY + 6})`}>
            {/* cargo box */}
            <rect x={-50} y={-30} width={60} height={42} rx={2}
                  fill="var(--eye)" />
            {/* cab */}
            <path d="M 10 -16 L 40 -16 L 50 -2 L 50 12 L 10 12 Z"
                  fill="var(--eye)" />
            <rect x={20} y={-10} width={20} height={10} fill="var(--bg)" opacity={0.7} />
            {/* wheels */}
            <g transform={`translate(-30 18) rotate(${wheelRot})`}>
              <circle cx={0} cy={0} r={8} fill="var(--accent)" />
              <circle cx={0} cy={0} r={3} fill="var(--bg)" />
            </g>
            <g transform={`translate(34 18) rotate(${wheelRot})`}>
              <circle cx={0} cy={0} r={8} fill="var(--accent)" />
              <circle cx={0} cy={0} r={3} fill="var(--bg)" />
            </g>
            {/* speed lines behind */}
            {[0, 1, 2].map((i) => (
              <line key={i}
                    x1={-60 - i * 14} y1={-6 + i * 8}
                    x2={-46 - i * 14} y2={-6 + i * 8}
                    stroke="var(--accent)" strokeWidth={2}
                    strokeLinecap="round" opacity={0.5 - i * 0.1} />
            ))}
          </g>
        );
      } },

    { id: 'delivery-status', label: 'Tracking', duration: 3600,
      render: (t) => {
        const progress = clamp(t * 1.1, 0, 1);
        const stops = [
          { x: W / 2 - 90, label: 'PICK' },
          { x: W / 2 - 30, label: 'WAY' },
          { x: W / 2 + 30, label: 'NEAR' },
          { x: W / 2 + 90, label: 'HOME' },
        ];
        return (
          <g>
            {/* progress track */}
            <line x1={W / 2 - 90} y1={CY} x2={W / 2 + 90} y2={CY}
                  stroke="var(--eye)" strokeWidth={3} opacity={0.4} />
            <line x1={W / 2 - 90} y1={CY}
                  x2={W / 2 - 90 + 180 * progress} y2={CY}
                  stroke="var(--accent)" strokeWidth={3} strokeLinecap="round" />
            {/* stops */}
            {stops.map((s, i) => {
              const reached = progress >= i / (stops.length - 1) - 0.02;
              return (
                <g key={i}>
                  <circle cx={s.x} cy={CY} r={6}
                          fill={reached ? 'var(--accent)' : 'var(--bg)'}
                          stroke="var(--eye)" strokeWidth={2} />
                  <text x={s.x} y={CY + 22} textAnchor="middle"
                        fontFamily="ui-monospace, Menlo, monospace"
                        fontSize={9} fontWeight={700}
                        fill="var(--eye)"
                        opacity={reached ? 0.9 : 0.45}>{s.label}</text>
                </g>
              );
            })}
            {/* moving box icon */}
            <g transform={`translate(${W / 2 - 90 + 180 * progress} ${CY - 24})`}>
              <rect x={-10} y={-8} width={20} height={16} rx={2}
                    fill="var(--accent)" />
              <line x1={0} y1={-8} x2={0} y2={8}
                    stroke="var(--bg)" strokeWidth={1.5} />
            </g>
            <text x={W / 2} y={STATUS_H + 30} textAnchor="middle"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={11} fontWeight={700}
                  fill="var(--eye)" letterSpacing={3}>TRACKING</text>
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: FLASHLIGHT — 2 variants
// =============================================================
cats.flashlight = {
  label: 'Flashlight',
  desc: 'Torch · light beam.',
  kind: 'scene',
  tone: 'warm',
  variants: [
    { id: 'flashlight-beam', label: 'Beam on', duration: 2400,
      render: (t) => {
        const flick = 0.85 + Math.sin(t * TAU * 6) * 0.05
                          + Math.sin(t * TAU * 11) * 0.04;
        return (
          <g>
            {/* torch body — angled top-left */}
            <g transform={`translate(${W / 2 - 70} ${CY + 50}) rotate(-30)`}>
              <rect x={0} y={-9} width={50} height={18} rx={2}
                    fill="var(--eye)" />
              <rect x={50} y={-12} width={14} height={24} rx={2}
                    fill="var(--accent)" />
              {/* button */}
              <circle cx={18} cy={0} r={3} fill="var(--bg)" opacity={0.7} />
            </g>
            {/* light cone */}
            <g opacity={flick}>
              <path d={`M ${W / 2 - 14} ${CY + 36}
                        L ${W} ${STATUS_H + 30}
                        L ${W} ${H - 60}
                        L ${W / 2 - 4} ${CY + 50} Z`}
                    fill="var(--accent)" opacity={0.25} />
              <path d={`M ${W / 2 - 14} ${CY + 36}
                        L ${W} ${STATUS_H + 50}
                        L ${W} ${H - 80}
                        L ${W / 2 - 6} ${CY + 46} Z`}
                    fill="var(--accent)" opacity={0.35} />
            </g>
            {/* dust motes in beam */}
            {Array.from({ length: 5 }).map((_, i) => {
              const seed = (i * 19) % 100 / 100;
              const p = ((t * 0.8) + seed) % 1;
              const cx = lerp(W / 2 + 10, W - 30, p);
              const cy = lerp(CY + 20, STATUS_H + 60, p) +
                         Math.sin(p * TAU + i) * 6;
              return (
                <circle key={i} cx={cx} cy={cy} r={1.5 + (i % 2)}
                        fill="var(--bg)" opacity={(1 - p) * 0.5} />
              );
            })}
          </g>
        );
      } },

    { id: 'flashlight-sweep', label: 'Sweep', duration: 3200,
      render: (t) => {
        const angle = Math.sin(t * TAU - Math.PI / 2) * 35;
        return (
          <g>
            {/* lamp at bottom */}
            <g transform={`translate(${W / 2} ${H - 14})`}>
              <rect x={-18} y={-6} width={36} height={12} rx={3}
                    fill="var(--eye)" />
              <circle cx={0} cy={-8} r={6} fill="var(--accent)" />
            </g>
            {/* sweeping beam */}
            <g transform={`translate(${W / 2} ${H - 16}) rotate(${angle})`}>
              <path d={`M 0 0 L -42 -130 L 42 -130 Z`}
                    fill="var(--accent)" opacity={0.28} />
              <path d={`M 0 0 L -22 -130 L 22 -130 Z`}
                    fill="var(--accent)" opacity={0.45} />
              <line x1={0} y1={0} x2={0} y2={-130}
                    stroke="var(--accent)" strokeWidth={2} opacity={0.7} />
            </g>
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: PODCAST — 2 variants (talk-radio mic + waveform, not music)
// =============================================================
cats.podcast = {
  label: 'Podcast',
  desc: 'Talk audio · mic · live.',
  kind: 'scene',
  tone: 'purple',
  variants: [
    { id: 'podcast-mic', label: 'Mic + waveform', duration: 2400,
      render: (t) => {
        const pulseM = 1 + Math.sin(t * TAU * 3) * 0.04;
        return (
          <g>
            {/* mic on left */}
            <g transform={`translate(70 ${CY}) scale(${pulseM})`}>
              <rect x={-14} y={-30} width={28} height={42} rx={14}
                    fill="var(--eye)" />
              {/* grille lines */}
              {[-22, -14, -6, 2].map((y, i) => (
                <line key={i} x1={-10} y1={y} x2={10} y2={y}
                      stroke="var(--bg)" strokeWidth={1} opacity={0.55} />
              ))}
              {/* stand */}
              <path d="M -16 8 Q -16 22, 0 22 Q 16 22, 16 8"
                    fill="none" stroke="var(--eye)" strokeWidth={3} />
              <line x1={0} y1={22} x2={0} y2={36}
                    stroke="var(--eye)" strokeWidth={3} />
              <line x1={-10} y1={36} x2={10} y2={36}
                    stroke="var(--eye)" strokeWidth={3} strokeLinecap="round" />
            </g>
            {/* waveform on right (continuous wiggly line, not bars) */}
            <g transform={`translate(140 ${CY})`} stroke="var(--accent)"
               strokeWidth={2.5} fill="none" strokeLinecap="round">
              <path d={(() => {
                let d = '';
                for (let i = 0; i <= 60; i++) {
                  const x = i * 2.3;
                  const v = Math.sin(i * 0.4 + t * TAU * 2) * 14 +
                            Math.sin(i * 0.13 - t * TAU * 1.3) * 6;
                  d += (i === 0 ? 'M' : 'L') + ` ${x} ${v} `;
                }
                return d;
              })()} />
            </g>
            {/* LIVE pill */}
            <g transform={`translate(${W - 50} ${STATUS_H + 28})`}>
              <rect x={-22} y={-10} width={44} height={18} rx={9}
                    fill="var(--accent)" />
              <circle cx={-14} cy={-1} r={3} fill="var(--bg)"
                      opacity={Math.sin(t * TAU * 2) > 0 ? 1 : 0.3} />
              <text x={3} y={3} textAnchor="middle"
                    fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={10} fontWeight={800}
                    fill="var(--bg)" letterSpacing={1}>LIVE</text>
            </g>
          </g>
        );
      } },

    { id: 'podcast-headphones', label: 'Headphones', duration: 3000,
      render: (t) => {
        const bob = Math.sin(t * TAU) * 1.5;
        return (
          <g transform={`translate(0 ${bob})`}>
            {/* headband arc */}
            <path d={`M ${W / 2 - 60} ${CY - 10}
                      Q ${W / 2} ${CY - 70}, ${W / 2 + 60} ${CY - 10}`}
                  fill="none" stroke="var(--eye)" strokeWidth={6}
                  strokeLinecap="round" />
            {/* ear cup left */}
            <g transform={`translate(${W / 2 - 60} ${CY + 14})`}>
              <rect x={-16} y={-24} width={32} height={48} rx={10}
                    fill="var(--eye)" />
              <rect x={-12} y={-20} width={24} height={40} rx={6}
                    fill="var(--accent)" />
            </g>
            {/* ear cup right */}
            <g transform={`translate(${W / 2 + 60} ${CY + 14})`}>
              <rect x={-16} y={-24} width={32} height={48} rx={10}
                    fill="var(--eye)" />
              <rect x={-12} y={-20} width={24} height={40} rx={6}
                    fill="var(--accent)" />
            </g>
            {/* audio waves around */}
            {[0, 1].map((i) => {
              const p = ((t * 1.5) + i * 0.5) % 1;
              return (
                <g key={i} opacity={1 - p}>
                  <path d={`M ${W / 2 - 84 - p * 14} ${CY}
                            Q ${W / 2 - 94 - p * 14} ${CY + 14},
                            ${W / 2 - 84 - p * 14} ${CY + 28}`}
                        fill="none" stroke="var(--accent)" strokeWidth={2.5}
                        strokeLinecap="round" />
                  <path d={`M ${W / 2 + 84 + p * 14} ${CY}
                            Q ${W / 2 + 94 + p * 14} ${CY + 14},
                            ${W / 2 + 84 + p * 14} ${CY + 28}`}
                        fill="none" stroke="var(--accent)" strokeWidth={2.5}
                        strokeLinecap="round" />
                </g>
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: STOPWATCH — 2 variants (count-UP timer; distinct from countdown)
// =============================================================
cats.stopwatch = {
  label: 'Stopwatch',
  desc: 'Count up · lap · running time.',
  kind: 'scene',
  tone: 'red',
  variants: [
    { id: 'stopwatch-running', label: 'Running', duration: 3000,
      render: (t) => {
        // simulate a counting-up digital readout
        const seconds = (t * 9.99);
        const wholeSec = Math.floor(seconds);
        const cs = Math.floor((seconds - wholeSec) * 100);
        const label = `00:${String(wholeSec).padStart(2, '0')}.${String(cs).padStart(2, '0')}`;
        return (
          <g>
            {/* stopwatch body */}
            <g transform={`translate(${W / 2} ${CY + 8})`}>
              <circle cx={0} cy={0} r={50}
                      fill="none" stroke="var(--eye)" strokeWidth={3.5} />
              {/* crown button */}
              <rect x={-4} y={-58} width={8} height={6} rx={1}
                    fill="var(--eye)" />
              {/* tick marks every 30deg */}
              {Array.from({ length: 12 }).map((_, i) => {
                const a = (i / 12) * TAU - Math.PI / 2;
                return (
                  <line key={i}
                        x1={Math.cos(a) * 42} y1={Math.sin(a) * 42}
                        x2={Math.cos(a) * 48} y2={Math.sin(a) * 48}
                        stroke="var(--eye)" strokeWidth={1.5} opacity={0.5} />
                );
              })}
              {/* center pivot */}
              <circle cx={0} cy={0} r={3} fill="var(--accent)" />
              {/* sweeping hand */}
              <line x1={0} y1={0}
                    x2={Math.cos(seconds / 10 * TAU - Math.PI / 2) * 38}
                    y2={Math.sin(seconds / 10 * TAU - Math.PI / 2) * 38}
                    stroke="var(--accent)" strokeWidth={2.5}
                    strokeLinecap="round" />
              {/* digital readout below center */}
              <text x={0} y={22} textAnchor="middle"
                    fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={11} fontWeight={700}
                    fill="var(--eye)" letterSpacing={1.5}>{label}</text>
            </g>
          </g>
        );
      } },

    { id: 'stopwatch-laps', label: 'Lap times', duration: 4800,
      render: (t) => {
        const laps = [
          '01  00:21.44',
          '02  00:19.87',
          '03  00:22.10',
        ];
        const shown = Math.min(laps.length, Math.floor(t * laps.length) + 1);
        const liveCs = ((t * 100) % 100);
        const live = `00:${String(Math.floor(t * 9.99) % 60).padStart(2, '0')}.${String(Math.floor(liveCs)).padStart(2, '0')}`;

        // Runner stick figure on the right — same body-language as walking-runner.
        const bob = Math.abs(Math.sin(t * TAU * 4)) * 3;
        const rcx = W - 56;
        const groundY = H - 24;
        const rcy = groundY - 36 - bob;
        const armA = Math.sin(t * TAU * 4) * 18;
        const legA = Math.sin(t * TAU * 4) * 16;

        return (
          <g>
            {/* live time top-left */}
            <text x={20} y={STATUS_H + 38}
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={22} fontWeight={800}
                  fill="var(--accent)" letterSpacing={2}>{live}</text>
            <text x={20} y={STATUS_H + 52}
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={9} fontWeight={700}
                  fill="var(--eye)" letterSpacing={3} opacity={0.6}>RUNNING</text>

            {/* lap rows on the left */}
            {laps.slice(0, shown).map((l, i) => (
              <g key={i} transform={`translate(20 ${STATUS_H + 80 + i * 18})`}>
                <line x1={0} y1={0} x2={140} y2={0}
                      stroke="var(--eye)" strokeWidth={0.5} opacity={0.3} />
                <text x={0} y={12} fontFamily="ui-monospace, Menlo, monospace"
                      fontSize={11} fontWeight={700}
                      fill="var(--eye)" opacity={0.85}>{l}</text>
              </g>
            ))}

            {/* Runner stick figure on the right */}
            <g stroke="var(--eye)" strokeWidth={3.5} strokeLinecap="round" fill="none">
              {/* ground */}
              <line x1={W - 110} y1={groundY} x2={W - 10} y2={groundY}
                    strokeWidth={1.5} opacity={0.4} />
              {/* head */}
              <circle cx={rcx} cy={rcy - 22} r={8} fill="var(--eye)" stroke="none" />
              {/* body */}
              <line x1={rcx} y1={rcy - 14} x2={rcx + 1} y2={rcy + 10} />
              {/* arms */}
              <line x1={rcx} y1={rcy - 8} x2={rcx - 10 + armA} y2={rcy + 2 - armA / 2} />
              <line x1={rcx} y1={rcy - 8} x2={rcx + 10 - armA} y2={rcy + 2 + armA / 2} />
              {/* legs */}
              <line x1={rcx + 1} y1={rcy + 10} x2={rcx - 7 + legA} y2={groundY - bob} />
              <line x1={rcx + 1} y1={rcy + 10} x2={rcx + 9 - legA} y2={groundY - bob} />
              {/* speed lines */}
              {[0, 1, 2].map((i) => {
                const p = ((t * 4) + i * 0.33) % 1;
                return (
                  <line key={i}
                        x1={rcx - 36 - p * 22} y1={rcy - 8 + i * 8}
                        x2={rcx - 22 - p * 16} y2={rcy - 8 + i * 8}
                        strokeWidth={2} opacity={(1 - p) * 0.7} />
                );
              })}
            </g>
          </g>
        );
      } },
  ],
};

// =============================================================
// Merge new keys
// =============================================================
(() => {
  const newEmotionKeys = ['brave', 'dreamy', 'awe'];
  const newSceneKeys   = ['delivery', 'flashlight', 'podcast', 'stopwatch'];
  const allNew = [...newEmotionKeys, ...newSceneKeys];

  const keys = (window.EMOTION_CATEGORY_KEYS || Object.keys(cats))
                 .filter((k) => !allNew.includes(k));

  let firstScene = keys.findIndex((k) => cats[k] && cats[k].kind === 'scene');
  if (firstScene === -1) firstScene = keys.length;
  keys.splice(firstScene, 0, ...newEmotionKeys.filter((k) => k in cats));
  keys.push(...newSceneKeys.filter((k) => k in cats));

  window.EMOTION_CATEGORY_KEYS = keys;
})();
