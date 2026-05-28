/* Luni — emotion + scene pack #2.
   Adds emotions: hot, lonely, grateful
   Adds scenes:   reading, smarthome, shopping, lock
   Loaded AFTER emotion-pack-luni.jsx.
*/

const {
  W, H, STATUS_H, CY, GAP, LX, RX, EYE_W, EYE_H, EYE_RX,
  TAU, lerp, clamp, wrap, ease, pulse, blink,
  Eye, Eyes, heartPath, starPath, arcPath,
} = window.EmotionCore;

const cats = window.EMOTION_CATEGORIES;

// =============================================================
// EMOTION: HOT — 3 variants
// =============================================================
cats.hot = {
  label: 'Hot',
  desc: 'Sweltering / overheated.',
  tone: 'red',
  variants: [
    { id: 'hot-sweat', label: 'Sweat drops', duration: 2200,
      render: (t) => {
        const droop = 2 + Math.sin(t * TAU * 0.6) * 1;
        const y = CY + droop;
        return (
          <g>
            <Eyes L={{ y, h: EYE_H * 0.65 }} R={{ y, h: EYE_H * 0.65 }} />
            {/* sweat beads dripping from above the eyes */}
            {[0, 1, 2].map((i) => {
              const seed = i * 0.33;
              const p = ((t * 1.2) + seed) % 1;
              const x = i === 0 ? LX - 24 : i === 1 ? W / 2 : RX + 24;
              const yPos = STATUS_H + 8 + p * 80;
              return (
                <g key={i} opacity={1 - p * 0.3}>
                  <path d={`M ${x} ${yPos}
                            q -5 -8, 0 -14 q 5 6, 0 14 Z`}
                        fill="var(--accent)" />
                  <ellipse cx={x - 1.5} cy={yPos - 5} rx={1.5} ry={2.5}
                           fill="var(--bg)" opacity={0.7} />
                </g>
              );
            })}
            {/* heat lines on right */}
            {[0, 1, 2].map((i) => {
              const wob = Math.sin(t * TAU * 2 + i) * 4;
              return (
                <path key={i}
                      d={`M ${W - 20 - i * 8} ${CY - 30}
                          q ${wob} 12, 0 24
                          q ${-wob} 12, 0 24`}
                      fill="none" stroke="var(--accent)" strokeWidth={2}
                      strokeLinecap="round" opacity={0.55} />
              );
            })}
          </g>
        );
      } },

    { id: 'hot-fan', label: 'Fan self', duration: 1400,
      render: (t) => {
        const swing = Math.sin(t * TAU * 3) * 24;
        return (
          <g>
            <Eyes L={{ h: EYE_H * 0.7 }} R={{ h: EYE_H * 0.7 }} />
            {/* hand-fan arc swinging across right side */}
            <g transform={`translate(${W - 36} ${CY + 8}) rotate(${swing})`}>
              <path d="M 0 0 L -22 -14 A 26 26 0 0 1 -22 14 Z"
                    fill="var(--accent)" />
              <line x1={0} y1={0} x2={-22} y2={-14}
                    stroke="var(--eye)" strokeWidth={1} opacity={0.5} />
              <line x1={0} y1={0} x2={-26} y2={0}
                    stroke="var(--eye)" strokeWidth={1} opacity={0.5} />
              <line x1={0} y1={0} x2={-22} y2={14}
                    stroke="var(--eye)" strokeWidth={1} opacity={0.5} />
            </g>
            {/* wind lines opposite */}
            {[0, 1, 2].map((i) => {
              const p = ((t * 2) + i * 0.3) % 1;
              return (
                <line key={i}
                      x1={W - 70 - p * 24} y1={CY - 16 + i * 16}
                      x2={W - 90 - p * 24} y2={CY - 16 + i * 16}
                      stroke="var(--accent)" strokeWidth={2.5}
                      strokeLinecap="round" opacity={1 - p} />
              );
            })}
          </g>
        );
      } },

    { id: 'hot-melt', label: 'Melting', duration: 3000,
      render: (t) => {
        const melt = (Math.sin(t * TAU - Math.PI / 2) + 1) / 2;
        const dropH = lerp(0, 22, melt);
        return (
          <g>
            {/* eyes melt down — bottoms stretched into drips */}
            {[LX, RX].map((cx, i) => (
              <g key={i}>
                <Eye x={cx} y={CY} h={EYE_H * (0.8 - melt * 0.15)} />
                {/* drip below */}
                <ellipse cx={cx - 14} cy={CY + EYE_H / 2 + dropH * 0.4}
                         rx={6} ry={dropH * 0.5} fill="var(--eye)" />
                <ellipse cx={cx + 14} cy={CY + EYE_H / 2 + dropH * 0.7}
                         rx={5} ry={dropH * 0.6} fill="var(--eye)" />
                <circle cx={cx - 14} cy={CY + EYE_H / 2 + dropH * 0.8}
                        r={4 + melt * 2} fill="var(--eye)" />
                <circle cx={cx + 14} cy={CY + EYE_H / 2 + dropH * 1.2}
                        r={3 + melt * 2} fill="var(--eye)" />
              </g>
            ))}
            {/* puddle at bottom */}
            <ellipse cx={W / 2} cy={H - 14}
                     rx={lerp(20, 70, melt)} ry={lerp(2, 6, melt)}
                     fill="var(--accent)" opacity={0.6} />
          </g>
        );
      } },
  ],
};

// =============================================================
// EMOTION: LONELY — 3 variants
// =============================================================
cats.lonely = {
  label: 'Lonely',
  desc: 'Alone / wanting company.',
  tone: 'blue',
  variants: [
    { id: 'lonely-sigh', label: 'Lonely sigh', duration: 4400,
      render: (t) => {
        const phase = Math.sin(t * TAU - Math.PI / 2);
        const droop = lerp(2, 8, (phase + 1) / 2);
        const y = CY + droop;
        return (
          <g>
            <Eyes L={{ y, h: EYE_H * 0.55, rx: 14 }}
                  R={{ y, h: EYE_H * 0.55, rx: 14 }} />
            {/* breath-out cloud */}
            {phase > 0.2 && (
              <g opacity={(phase - 0.2) * 1.2} fill="var(--accent)">
                {[0, 1, 2].map((i) => (
                  <circle key={i}
                          cx={W / 2 + (i - 1) * 10}
                          cy={H - 24 + (phase - 0.2) * 14}
                          r={4 + (1 - Math.abs(i - 1) * 0.3) * 2}
                          opacity={0.5} />
                ))}
              </g>
            )}
            {/* faint dotted ring around eyes = isolation */}
            <ellipse cx={W / 2} cy={CY + 8} rx={120} ry={56}
                     fill="none" stroke="var(--accent)" strokeWidth={1}
                     strokeDasharray="2 6" opacity={0.35} />
          </g>
        );
      } },

    { id: 'lonely-empty', label: 'Empty room', duration: 5200,
      render: (t) => {
        const driftX = Math.sin(t * TAU * 0.4) * 4;
        return (
          <g>
            {/* one solo eye centered */}
            <Eye x={W / 2 + driftX} y={CY} w={EYE_W * 0.9}
                 h={EYE_H * 0.6} rx={16} />
            {/* faint outline of where the other eye "should be" */}
            <ellipse cx={LX} cy={CY} rx={EYE_W / 2} ry={EYE_H * 0.3}
                     fill="none" stroke="var(--eye)" strokeWidth={1}
                     opacity={0.2} strokeDasharray="3 5" />
            <ellipse cx={RX} cy={CY} rx={EYE_W / 2} ry={EYE_H * 0.3}
                     fill="none" stroke="var(--eye)" strokeWidth={1}
                     opacity={0.2} strokeDasharray="3 5" />
            {/* dim status text */}
            <text x={W / 2} y={H - 14} textAnchor="middle"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={10} fontWeight={600}
                  fill="var(--accent)" letterSpacing={3} opacity={0.6}>
              ALONE
            </text>
          </g>
        );
      } },

    { id: 'lonely-window', label: 'At the window', duration: 4800,
      render: (t) => {
        const droop = 4;
        const y = CY + droop;
        return (
          <g>
            {/* window frame */}
            <g stroke="var(--accent)" strokeWidth={2} fill="none" opacity={0.45}>
              <rect x={W / 2 - 70} y={STATUS_H + 14}
                    width={140} height={H - STATUS_H - 32} rx={2} />
              <line x1={W / 2} y1={STATUS_H + 14} x2={W / 2} y2={H - 18} />
              <line x1={W / 2 - 70} y1={CY} x2={W / 2 + 70} y2={CY} />
            </g>
            {/* eyes look down through window */}
            <Eyes L={{ y, h: EYE_H * 0.55 }}
                  R={{ y, h: EYE_H * 0.55 }} />
            <circle cx={LX} cy={y + 12} r={9} fill="var(--bg)" />
            <circle cx={RX} cy={y + 12} r={9} fill="var(--bg)" />
            {/* falling rain */}
            {Array.from({ length: 6 }).map((_, i) => {
              const seed = (i * 17) % 100 / 100;
              const p = ((t * 1.4) + seed) % 1;
              const x = (W / 2 - 60) + (i * 24) + Math.sin(seed * TAU) * 4;
              return (
                <line key={i}
                      x1={x} y1={STATUS_H + 16 + p * (H - STATUS_H - 40)}
                      x2={x - 2} y2={STATUS_H + 22 + p * (H - STATUS_H - 40)}
                      stroke="var(--accent)" strokeWidth={1.5}
                      opacity={(1 - p) * 0.65} />
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// EMOTION: GRATEFUL — 2 variants
// =============================================================
cats.grateful = {
  label: 'Grateful',
  desc: 'Thankful / heartfelt.',
  tone: 'rose',
  variants: [
    { id: 'grateful-bow', label: 'Deep bow', duration: 3200,
      render: (t) => {
        const phase = Math.sin(t * TAU - Math.PI / 2);
        const tilt = lerp(0, 22, (phase + 1) / 2);
        const dy = lerp(0, 14, (phase + 1) / 2);
        return (
          <g transform={`rotate(${tilt} ${W / 2} ${H + 60}) translate(0 ${dy})`}>
            <g fill="none" stroke="var(--eye)" strokeWidth={12}
               strokeLinecap="round">
              <path d={arcPath(LX, CY + 4, EYE_W - 4, 22, true)} />
              <path d={arcPath(RX, CY + 4, EYE_W - 4, 22, true)} />
            </g>
            {/* small heart between eyes */}
            <path d={heartPath(W / 2, CY - 6, 18)}
                  fill="var(--accent)" />
            {/* gentle mouth */}
            <path d={arcPath(W / 2, H - 32, 28, 5, true)}
                  fill="none" stroke="var(--eye)" strokeWidth={4}
                  strokeLinecap="round" opacity={0.7} />
          </g>
        );
      } },

    { id: 'grateful-sparkle', label: 'Heart sparkle', duration: 2400,
      render: (t) => (
        <g>
          {/* warm eyes */}
          <Eyes L={{ y: CY - 4, h: EYE_H * 0.8 }}
                R={{ y: CY - 4, h: EYE_H * 0.8 }} />
          {/* heart cheek pads */}
          <path d={heartPath(LX, CY + 34, 12)} fill="var(--accent)" opacity={0.85} />
          <path d={heartPath(RX, CY + 34, 12)} fill="var(--accent)" opacity={0.85} />
          {/* sparkles drifting up */}
          {[0, 1, 2, 3].map((i) => {
            const seed = i * 0.25;
            const p = ((t + seed) % 1);
            const baseX = 40 + i * 80;
            const x = baseX + Math.sin(p * TAU + i) * 6;
            const y = lerp(H - 30, STATUS_H + 12, p);
            const s = lerp(8, 2, p);
            return (
              <path key={i} d={starPath(x, y, s, s * 0.4)}
                    fill="var(--accent)" opacity={1 - p} />
            );
          })}
          {/* THX text */}
          <text x={W / 2} y={H - 14} textAnchor="middle"
                fontFamily="ui-monospace, Menlo, monospace"
                fontSize={10} fontWeight={700}
                fill="var(--eye)" letterSpacing={4} opacity={0.75}>
            THANK YOU
          </text>
        </g>
      ) },
  ],
};

// =============================================================
// SCENE: READING — 3 variants
// =============================================================
cats.reading = {
  label: 'Reading',
  desc: 'Book / story / e-paper.',
  kind: 'scene',
  tone: 'warm',
  variants: [
    { id: 'reading-book', label: 'Open book', duration: 3600,
      render: (t) => {
        const flip = ((t * 1.2) % 1);
        return (
          <g>
            {/* book spine center */}
            <line x1={W / 2} y1={STATUS_H + 18} x2={W / 2} y2={H - 20}
                  stroke="var(--eye)" strokeWidth={2} opacity={0.7} />
            {/* left page */}
            <rect x={W / 2 - 110} y={STATUS_H + 16}
                  width={108} height={H - STATUS_H - 36} rx={2}
                  fill="none" stroke="var(--eye)" strokeWidth={2} />
            {/* right page */}
            <rect x={W / 2 + 2} y={STATUS_H + 16}
                  width={108} height={H - STATUS_H - 36} rx={2}
                  fill="none" stroke="var(--eye)" strokeWidth={2} />
            {/* text lines — left */}
            {[0, 1, 2, 3, 4, 5].map((i) => (
              <rect key={`l${i}`}
                    x={W / 2 - 100} y={STATUS_H + 30 + i * 14}
                    width={i === 5 ? 56 : 90} height={3} rx={1}
                    fill="var(--eye)" opacity={0.55} />
            ))}
            {/* text lines — right */}
            {[0, 1, 2, 3, 4, 5].map((i) => (
              <rect key={`r${i}`}
                    x={W / 2 + 10} y={STATUS_H + 30 + i * 14}
                    width={i === 5 ? 40 : 90} height={3} rx={1}
                    fill="var(--eye)" opacity={0.55} />
            ))}
            {/* turning page (right side curls) */}
            {flip < 0.55 && (
              <path d={`M ${W / 2 + 2} ${STATUS_H + 16}
                        L ${W / 2 + 2 + 108 * (1 - flip * 1.8)} ${STATUS_H + 16}
                        L ${W / 2 + 2 + 108 * (1 - flip * 1.8) - 16} ${H - 20}
                        L ${W / 2 + 2} ${H - 20} Z`}
                    fill="var(--bg)" stroke="var(--eye)" strokeWidth={2}
                    opacity={0.85} />
            )}
          </g>
        );
      } },

    { id: 'reading-bookmark', label: 'Bookmark', duration: 2800,
      render: (t) => {
        const wobble = Math.sin(t * TAU) * 1.5;
        return (
          <g>
            {/* book cover */}
            <rect x={W / 2 - 60} y={STATUS_H + 16}
                  width={120} height={H - STATUS_H - 36} rx={4}
                  fill="var(--eye)" />
            {/* title bar */}
            <rect x={W / 2 - 40} y={STATUS_H + 36}
                  width={80} height={4} rx={2} fill="var(--bg)" opacity={0.7} />
            <rect x={W / 2 - 28} y={STATUS_H + 48}
                  width={56} height={3} rx={1.5} fill="var(--bg)" opacity={0.5} />
            {/* big star/ornament */}
            <path d={starPath(W / 2, CY + 6, 16, 7)}
                  fill="var(--bg)" opacity={0.85} />
            {/* bookmark ribbon sticking out top */}
            <g transform={`translate(${W / 2 + 28 + wobble} ${STATUS_H + 16})`}>
              <path d="M 0 0 L 12 0 L 12 28 L 6 22 L 0 28 Z"
                    fill="var(--accent)" />
            </g>
          </g>
        );
      } },

    { id: 'reading-storytime', label: 'Story time', duration: 2400,
      render: (t) => {
        const wob = Math.sin(t * TAU) * 2;
        return (
          <g>
            {/* small book at left */}
            <g transform={`translate(54 ${CY})`}>
              <rect x={-24} y={-30} width={48} height={60} rx={2}
                    fill="var(--eye)" />
              <line x1={0} y1={-30} x2={0} y2={30}
                    stroke="var(--bg)" strokeWidth={1.5} opacity={0.7} />
              {[0, 1, 2, 3].map((i) => (
                <line key={i} x1={-20 + (i % 2) * 22} y1={-20 + Math.floor(i / 2) * 16}
                      x2={-6 + (i % 2) * 22} y2={-20 + Math.floor(i / 2) * 16}
                      stroke="var(--bg)" strokeWidth={1} opacity={0.6} />
              ))}
            </g>
            {/* speech bubble with story words */}
            <g transform={`translate(${W / 2 + 30} ${CY - 16}) translate(0 ${wob})`}>
              <path d="M -64 -20 Q -70 -22, -70 -14 L -70 14 Q -70 22, -62 22
                       L -50 22 L -56 32 L -42 22 L 60 22 Q 70 22, 70 14
                       L 70 -14 Q 70 -22, 60 -22 L -62 -22 Z"
                    fill="var(--eye)" />
              {[0, 1, 2].map((i) => (
                <rect key={i} x={-58} y={-14 + i * 10}
                      width={i === 2 ? 60 : 110} height={3} rx={1.5}
                      fill="var(--bg)" opacity={0.75} />
              ))}
            </g>
            {/* ABC sparkles */}
            {[0, 1].map((i) => {
              const p = ((t * 1.2) + i * 0.5) % 1;
              const letters = ['A', 'b', 'c'];
              return (
                <text key={i}
                      x={W - 30 - i * 12}
                      y={STATUS_H + 28 + p * 20}
                      fontFamily="ui-monospace, Menlo, monospace"
                      fontSize={14 - p * 4} fontWeight={700}
                      fill="var(--accent)" opacity={1 - p}>
                  {letters[i] || 'A'}
                </text>
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: SMART HOME — 3 variants
// =============================================================
cats.smarthome = {
  label: 'Smart home',
  desc: 'Lights · devices · thermostat.',
  kind: 'scene',
  tone: 'warm',
  variants: [
    { id: 'smarthome-bulb', label: 'Bulb toggle', duration: 2800,
      render: (t) => {
        const onPhase = Math.sin(t * TAU) > 0;
        const onAmp = onPhase ? 1 : 0.2;
        return (
          <g>
            {/* bulb glass */}
            <g transform={`translate(${W / 2} ${CY - 10})`}>
              <path d="M -24 -10
                       Q -24 -36, 0 -36
                       Q 24 -36, 24 -10
                       Q 24 8, 12 20
                       L -12 20
                       Q -24 8, -24 -10 Z"
                    fill="var(--eye)" opacity={onAmp} />
              {/* filament */}
              <path d="M -10 6 Q -6 -6, 0 -4 Q 6 -2, 10 6"
                    fill="none" stroke="var(--bg)" strokeWidth={1.5}
                    opacity={onAmp * 0.8} />
              {/* base */}
              <rect x={-12} y={20} width={24} height={6}
                    fill="var(--accent)" />
              <rect x={-10} y={26} width={20} height={4}
                    fill="var(--accent)" opacity={0.7} />
              <rect x={-8} y={30} width={16} height={3}
                    fill="var(--accent)" opacity={0.5} />
            </g>
            {/* light rays */}
            {onPhase && [0, 1, 2, 3, 4, 5].map((i) => {
              const a = (i / 6) * TAU + t * 0.5;
              const r1 = 36, r2 = 56;
              const x1 = W / 2 + Math.cos(a) * r1;
              const y1 = CY - 10 + Math.sin(a) * r1;
              const x2 = W / 2 + Math.cos(a) * r2;
              const y2 = CY - 10 + Math.sin(a) * r2;
              return (
                <line key={i} x1={x1} y1={y1} x2={x2} y2={y2}
                      stroke="var(--accent)" strokeWidth={3}
                      strokeLinecap="round" opacity={0.7} />
              );
            })}
            {/* status text */}
            <text x={W / 2} y={H - 14} textAnchor="middle"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={11} fontWeight={700}
                  fill="var(--eye)" letterSpacing={3}>
              {onPhase ? 'LIGHTS ON' : 'LIGHTS OFF'}
            </text>
          </g>
        );
      } },

    { id: 'smarthome-thermostat', label: 'Thermostat', duration: 3600,
      render: (t) => {
        const temp = Math.round(lerp(68, 76, (Math.sin(t * TAU) + 1) / 2));
        const sweep = ((t * 0.7) % 1);
        return (
          <g>
            <g transform={`translate(${W / 2} ${CY - 4})`}>
              {/* dial outer ring */}
              <circle cx={0} cy={0} r={52} fill="none"
                      stroke="var(--eye)" strokeWidth={3} opacity={0.55} />
              {/* arc sweep */}
              <path d={`M ${Math.cos(Math.PI * 0.75) * 52}
                          ${Math.sin(Math.PI * 0.75) * 52}
                        A 52 52 0 ${sweep > 0.5 ? 1 : 0} 1
                          ${Math.cos(Math.PI * 0.75 + sweep * 1.5 * Math.PI) * 52}
                          ${Math.sin(Math.PI * 0.75 + sweep * 1.5 * Math.PI) * 52}`}
                    fill="none" stroke="var(--accent)" strokeWidth={4}
                    strokeLinecap="round" />
              {/* tick marks */}
              {Array.from({ length: 12 }).map((_, i) => {
                const a = (i / 12) * TAU - Math.PI / 2;
                return (
                  <line key={i}
                        x1={Math.cos(a) * 44} y1={Math.sin(a) * 44}
                        x2={Math.cos(a) * 50} y2={Math.sin(a) * 50}
                        stroke="var(--eye)" strokeWidth={1.5} opacity={0.5} />
                );
              })}
              {/* temp text */}
              <text x={0} y={2} textAnchor="middle"
                    fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={28} fontWeight={800} fill="var(--eye)">{temp}°</text>
              <text x={0} y={20} textAnchor="middle"
                    fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={9} fontWeight={600}
                    fill="var(--eye)" letterSpacing={2} opacity={0.65}>HOME</text>
            </g>
          </g>
        );
      } },

    { id: 'smarthome-house', label: 'House map', duration: 3000,
      render: (t) => (
        <g>
          {/* house outline */}
          <g transform={`translate(${W / 2} ${CY})`}
             stroke="var(--eye)" strokeWidth={2.5} fill="none">
            <path d="M -60 30 L -60 -10 L 0 -50 L 60 -10 L 60 30 Z" />
            <line x1={-60} y1={-10} x2={60} y2={-10} />
            <line x1={0} y1={-10} x2={0} y2={30} />
            <rect x={-12} y={10} width={24} height={20} />
          </g>
          {/* room indicators (dots that light up in sequence) */}
          {[
            { x: -32, y: -28 },
            { x: 32, y: -28 },
            { x: -32, y: 10 },
            { x: 32, y: 10 },
          ].map((p, i) => {
            const phase = ((t * 2) - i * 0.25 + 4) % 4;
            const on = phase < 1;
            return (
              <circle key={i} cx={W / 2 + p.x} cy={CY + p.y} r={5}
                      fill={on ? 'var(--accent)' : 'var(--eye)'}
                      opacity={on ? 1 : 0.35} />
            );
          })}
          <text x={W / 2} y={H - 14} textAnchor="middle"
                fontFamily="ui-monospace, Menlo, monospace"
                fontSize={10} fontWeight={700}
                fill="var(--eye)" letterSpacing={3} opacity={0.7}>
            4 ROOMS · 2 ACTIVE
          </text>
        </g>
      ) },
  ],
};

// =============================================================
// SCENE: SHOPPING — 2 variants
// =============================================================
cats.shopping = {
  label: 'Shopping',
  desc: 'Bag · cart · grocery list.',
  kind: 'scene',
  tone: 'green',
  variants: [
    { id: 'shopping-bag', label: 'Bag fill', duration: 2800,
      render: (t) => {
        const items = Math.floor(((t * 1.2) % 1) * 4) + 1;
        return (
          <g>
            {/* bag handles */}
            <g transform={`translate(${W / 2} ${CY})`}>
              <path d="M -22 -16 Q -22 -38, 0 -38 Q 22 -38, 22 -16"
                    fill="none" stroke="var(--eye)" strokeWidth={4} />
              {/* bag body */}
              <path d="M -36 -14 L 36 -14 L 30 40 L -30 40 Z"
                    fill="var(--eye)" />
              {/* items pop in */}
              <g>
                {items >= 1 && (
                  <circle cx={-14} cy={26} r={6} fill="var(--accent)" />
                )}
                {items >= 2 && (
                  <rect x={4} y={20} width={14} height={14} rx={2}
                        fill="var(--accent)" opacity={0.8} />
                )}
                {items >= 3 && (
                  <path d={starPath(-10, 2, 7, 3)} fill="var(--accent)" opacity={0.85} />
                )}
                {items >= 4 && (
                  <path d={heartPath(16, 4, 12)} fill="var(--bg)" opacity={0.9} />
                )}
              </g>
            </g>
            {/* counter */}
            <text x={W / 2 + 56} y={STATUS_H + 36}
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={14} fontWeight={700}
                  fill="var(--accent)">{items}/4</text>
          </g>
        );
      } },

    { id: 'shopping-list', label: 'Grocery list', duration: 3600,
      render: (t) => {
        const checked = Math.floor((t * 4) % 4);
        return (
          <g>
            {/* paper */}
            <rect x={W / 2 - 70} y={STATUS_H + 14}
                  width={140} height={H - STATUS_H - 32} rx={4}
                  fill="var(--eye)" />
            {/* binder holes */}
            {[0, 1, 2].map((i) => (
              <circle key={i} cx={W / 2 - 60} cy={STATUS_H + 30 + i * 50}
                      r={3} fill="var(--bg)" />
            ))}
            {/* list lines + checkboxes */}
            {[0, 1, 2, 3].map((i) => (
              <g key={i} transform={`translate(${W / 2 - 40} ${STATUS_H + 32 + i * 22})`}>
                <rect x={0} y={-7} width={12} height={12} rx={2}
                      fill="none" stroke="var(--bg)" strokeWidth={1.5} />
                {i < checked && (
                  <path d="M 2 -1 L 5 3 L 11 -5"
                        fill="none" stroke="var(--accent)" strokeWidth={2.5}
                        strokeLinecap="round" strokeLinejoin="round" />
                )}
                <rect x={20} y={-2.5}
                      width={i === 3 ? 60 : 80} height={3} rx={1.5}
                      fill="var(--bg)" opacity={i < checked ? 0.35 : 0.7} />
                {i < checked && (
                  <line x1={20} y1={-1} x2={i === 3 ? 80 : 100} y2={-1}
                        stroke="var(--bg)" strokeWidth={1.5} opacity={0.5} />
                )}
              </g>
            ))}
            {/* total at bottom of list */}
            <text x={W / 2 + 50} y={H - 22} textAnchor="end"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={10} fontWeight={700}
                  fill="var(--accent)">{checked}/4 ✓</text>
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: LOCK — 3 variants
// =============================================================
cats.lock = {
  label: 'Lock',
  desc: 'Locked · unlocked · fingerprint.',
  kind: 'scene',
  tone: 'cyan',
  variants: [
    { id: 'lock-closed', label: 'Locked', duration: 2400,
      render: (t) => {
        const shake = Math.sin(t * TAU * 1.5) * 1.2;
        return (
          <g transform={`translate(${shake} 0)`}>
            {/* shackle */}
            <path d={`M ${W / 2 - 22} ${CY - 6}
                      L ${W / 2 - 22} ${CY - 26}
                      A 22 22 0 0 1 ${W / 2 + 22} ${CY - 26}
                      L ${W / 2 + 22} ${CY - 6}`}
                  fill="none" stroke="var(--eye)" strokeWidth={6}
                  strokeLinecap="round" />
            {/* body */}
            <rect x={W / 2 - 34} y={CY - 6}
                  width={68} height={56} rx={6} fill="var(--eye)" />
            {/* keyhole */}
            <circle cx={W / 2} cy={CY + 18} r={5} fill="var(--bg)" />
            <rect x={W / 2 - 2} y={CY + 18} width={4} height={12}
                  fill="var(--bg)" />
            <text x={W / 2} y={H - 14} textAnchor="middle"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={11} fontWeight={700}
                  fill="var(--eye)" letterSpacing={3}>LOCKED</text>
          </g>
        );
      } },

    { id: 'lock-open', label: 'Unlocked', duration: 2000,
      render: (t) => {
        const lift = pulse(t, 0.4, 0.3);
        return (
          <g>
            {/* shackle — tilted/lifted open on left side */}
            <g transform={`translate(${-12 - lift * 4} ${-lift * 4})`}>
              <path d={`M ${W / 2 - 22} ${CY - 6}
                        L ${W / 2 - 22} ${CY - 26}
                        A 22 22 0 0 1 ${W / 2 + 22} ${CY - 26}
                        L ${W / 2 + 22} ${CY - 6}`}
                    fill="none" stroke="var(--eye)" strokeWidth={6}
                    strokeLinecap="round" />
            </g>
            {/* body */}
            <rect x={W / 2 - 34} y={CY - 6}
                  width={68} height={56} rx={6} fill="var(--accent)" />
            {/* check */}
            <path d={`M ${W / 2 - 14} ${CY + 22}
                      L ${W / 2 - 4} ${CY + 32}
                      L ${W / 2 + 16} ${CY + 12}`}
                  fill="none" stroke="var(--bg)" strokeWidth={5}
                  strokeLinecap="round" strokeLinejoin="round" />
            <text x={W / 2} y={H - 14} textAnchor="middle"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={11} fontWeight={700}
                  fill="var(--accent)" letterSpacing={3}>UNLOCKED</text>
          </g>
        );
      } },

    { id: 'lock-fingerprint', label: 'Fingerprint', duration: 2800,
      render: (t) => {
        const scan = (t % 1);
        return (
          <g>
            {/* fingerprint concentric arcs */}
            <g transform={`translate(${W / 2} ${CY})`}>
              {[0, 1, 2, 3, 4].map((i) => {
                const r = 12 + i * 7;
                const op = scan * 5 >= i ? 1 : 0.25;
                return (
                  <path key={i}
                        d={`M ${-r} 4 A ${r} ${r * 1.2} 0 0 1 ${r} 4`}
                        fill="none" stroke="var(--eye)" strokeWidth={2.5}
                        opacity={op} strokeLinecap="round" />
                );
              })}
              {/* center dot */}
              <circle cx={0} cy={4} r={3} fill="var(--accent)" />
            </g>
            {/* scan line moving down */}
            <line x1={W / 2 - 50}
                  y1={STATUS_H + 24 + scan * 100}
                  x2={W / 2 + 50}
                  y2={STATUS_H + 24 + scan * 100}
                  stroke="var(--accent)" strokeWidth={2} opacity={0.7} />
            <text x={W / 2} y={H - 14} textAnchor="middle"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={10} fontWeight={700}
                  fill="var(--eye)" letterSpacing={3} opacity={0.7}>
              SCANNING…
            </text>
          </g>
        );
      } },
  ],
};

// =============================================================
// Merge new keys into the public KEYS list
// =============================================================
(() => {
  const newEmotionKeys = ['hot', 'lonely', 'grateful'];
  const newSceneKeys   = ['reading', 'smarthome', 'shopping', 'lock'];
  const allNew = [...newEmotionKeys, ...newSceneKeys];

  const keys = (window.EMOTION_CATEGORY_KEYS || Object.keys(cats))
                 .filter((k) => !allNew.includes(k));

  // emotions before scenes
  let firstScene = keys.findIndex((k) => cats[k] && cats[k].kind === 'scene');
  if (firstScene === -1) firstScene = keys.length;
  keys.splice(firstScene, 0, ...newEmotionKeys.filter((k) => k in cats));
  keys.push(...newSceneKeys.filter((k) => k in cats));

  window.EMOTION_CATEGORY_KEYS = keys;
})();
