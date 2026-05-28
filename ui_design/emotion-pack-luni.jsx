/* Luni — emotion + scene pack expansion.
   Adds emotions: sick, cold, calm, playful
   Adds scenes:   alarm, notification, birthday, reminder, bluetooth, video
   Loaded AFTER emotion-color.jsx, so we set tones inline.
*/

const {
  W, H, STATUS_H, CY, GAP, LX, RX, EYE_W, EYE_H, EYE_RX,
  TAU, lerp, clamp, wrap, ease, pulse, blink,
  Eye, Eyes, heartPath, starPath, arcPath,
} = window.EmotionCore;

const cats = window.EMOTION_CATEGORIES;

// =============================================================
// EMOTION: SICK — 3 variants
// =============================================================
cats.sick = {
  label: 'Sick',
  desc: 'Feverish / unwell.',
  tone: 'green',
  variants: [
    { id: 'sick-fever', label: 'Fever', duration: 2800,
      render: (t) => {
        const droop = 4 + Math.sin(t * TAU * 0.5) * 1.5;
        const y = CY + droop;
        const sweat = (t * 1.4) % 1;
        return (
          <g>
            <Eyes L={{ y, h: EYE_H * 0.55, rx: 14 }}
                  R={{ y, h: EYE_H * 0.55, rx: 14 }} />
            {/* sweat drop on forehead */}
            <g opacity={1 - sweat * 0.4}>
              <path d={`M ${W/2 + 28} ${STATUS_H + 18 + sweat * 40}
                        q -6 -8, 0 -14 q 6 6, 0 14 Z`}
                    fill="var(--accent)" />
            </g>
            {/* thermometer bar across top */}
            <g transform={`translate(${W/2 - 36}, ${STATUS_H + 10})`}>
              <rect x={0} y={0} width={72} height={6} rx={3}
                    fill="none" stroke="var(--eye)" strokeWidth={1.5} opacity={0.5} />
              <rect x={2} y={2} width={lerp(40, 64, (Math.sin(t * TAU * 2) + 1) / 2)}
                    height={2} rx={1} fill="var(--accent)" />
            </g>
            {/* downturned mouth */}
            <path d={arcPath(W/2, H - 30, 30, 5)}
                  fill="none" stroke="var(--eye)" strokeWidth={4}
                  strokeLinecap="round" opacity={0.7} />
          </g>
        );
      } },

    { id: 'sick-queasy', label: 'Queasy', duration: 2400,
      render: (t) => {
        const wob = Math.sin(t * TAU * 1.5) * 5;
        const ty = Math.cos(t * TAU * 1.5) * 3;
        return (
          <g>
            <Eyes L={{ x: LX + wob, y: CY + ty, h: EYE_H * 0.6 }}
                  R={{ x: RX - wob, y: CY - ty, h: EYE_H * 0.6 }} />
            {/* wavy nausea line at bottom */}
            <g fill="none" stroke="var(--accent)" strokeWidth={3.5}
               strokeLinecap="round">
              <path d={`M 60 ${H - 28}
                        Q 90 ${H - 28 + Math.sin(t * TAU * 2) * 6}, 120 ${H - 28}
                        T 180 ${H - 28}
                        T 240 ${H - 28}
                        T 260 ${H - 28}`} />
            </g>
          </g>
        );
      } },

    { id: 'sick-mask', label: 'Mask', duration: 3200,
      render: (t) => {
        const breath = Math.sin(t * TAU) * 1.5;
        return (
          <g>
            <Eyes L={{ y: CY - 8, h: EYE_H * 0.7 }}
                  R={{ y: CY - 8, h: EYE_H * 0.7 }} />
            {/* surgical mask shape across lower half */}
            <g>
              <rect x={W/2 - 70} y={H - 60 + breath} width={140} height={42}
                    rx={20} fill="var(--eye)" opacity={0.85} />
              {/* mask fold lines */}
              <line x1={W/2 - 60} y1={H - 46 + breath} x2={W/2 + 60} y2={H - 46 + breath}
                    stroke="var(--bg)" strokeWidth={1} opacity={0.5} />
              <line x1={W/2 - 60} y1={H - 36 + breath} x2={W/2 + 60} y2={H - 36 + breath}
                    stroke="var(--bg)" strokeWidth={1} opacity={0.5} />
              {/* ear loops */}
              <path d={`M ${W/2 - 70} ${H - 50 + breath}
                        Q ${W/2 - 96} ${H - 40 + breath}, ${W/2 - 70} ${H - 28 + breath}`}
                    stroke="var(--eye)" strokeWidth={2} fill="none" opacity={0.7} />
              <path d={`M ${W/2 + 70} ${H - 50 + breath}
                        Q ${W/2 + 96} ${H - 40 + breath}, ${W/2 + 70} ${H - 28 + breath}`}
                    stroke="var(--eye)" strokeWidth={2} fill="none" opacity={0.7} />
            </g>
          </g>
        );
      } },
  ],
};

// =============================================================
// EMOTION: COLD — 2 variants
// =============================================================
cats.cold = {
  label: 'Cold',
  desc: 'Shivering / freezing.',
  tone: 'blue',
  variants: [
    { id: 'cold-shiver', label: 'Shiver', duration: 700,
      render: (t) => {
        const sh = Math.sin(t * TAU * 10) * 3;
        const sv = Math.cos(t * TAU * 10) * 2;
        return (
          <g transform={`translate(${sh} ${sv})`}>
            <Eyes L={{ h: EYE_H * 0.6, rx: 16 }}
                  R={{ h: EYE_H * 0.6, rx: 16 }} />
            {/* chattering teeth (zigzag bar) */}
            <path d={`M ${W/2 - 24} ${H - 30}
                      l 6 4 l 6 -4 l 6 4 l 6 -4 l 6 4 l 6 -4`}
                  fill="none" stroke="var(--eye)" strokeWidth={3}
                  strokeLinecap="round" />
          </g>
        );
      } },

    { id: 'cold-snowflakes', label: 'Snowflakes', duration: 3000,
      render: (t) => {
        const sh = Math.sin(t * TAU * 8) * 1.5;
        return (
          <g>
            <g transform={`translate(${sh} 0)`}>
              <Eyes L={{ h: EYE_H * 0.65 }}
                    R={{ h: EYE_H * 0.65 }} />
            </g>
            {/* drifting snowflakes */}
            {[0, 1, 2, 3, 4].map((i) => {
              const seed = i * 0.21;
              const p = ((t + seed) % 1);
              const x = 30 + (i * 56) % (W - 60) + Math.sin(p * TAU + i) * 8;
              const y = lerp(STATUS_H + 4, H - 8, p);
              const r = 3 + (i % 2) * 1;
              return (
                <g key={i} transform={`translate(${x} ${y}) rotate(${p * 360})`}
                   opacity={1 - p * 0.3}>
                  <line x1={-r} y1={0} x2={r} y2={0}
                        stroke="var(--eye)" strokeWidth={1.5} strokeLinecap="round" />
                  <line x1={0} y1={-r} x2={0} y2={r}
                        stroke="var(--eye)" strokeWidth={1.5} strokeLinecap="round" />
                  <line x1={-r * 0.7} y1={-r * 0.7} x2={r * 0.7} y2={r * 0.7}
                        stroke="var(--eye)" strokeWidth={1.2} strokeLinecap="round" opacity={0.7} />
                  <line x1={r * 0.7} y1={-r * 0.7} x2={-r * 0.7} y2={r * 0.7}
                        stroke="var(--eye)" strokeWidth={1.2} strokeLinecap="round" opacity={0.7} />
                </g>
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// EMOTION: CALM — 3 variants
// =============================================================
cats.calm = {
  label: 'Calm',
  desc: 'Meditation / serene.',
  tone: 'cyan',
  variants: [
    { id: 'calm-breathe', label: 'Slow breathe', duration: 6000,
      render: (t) => {
        const phase = (Math.sin(t * TAU - Math.PI / 2) + 1) / 2;
        const h = lerp(10, 22, phase);
        return (
          <g fill="none" stroke="var(--eye)" strokeWidth={lerp(10, 13, phase)}
             strokeLinecap="round">
            <path d={arcPath(LX, CY, EYE_W - 8, h)} />
            <path d={arcPath(RX, CY, EYE_W - 8, h)} />
            {/* breathing dot */}
            <circle cx={W / 2} cy={H - 28} r={lerp(3, 6, phase)}
                    fill="var(--accent)" opacity={0.7} />
          </g>
        );
      } },

    { id: 'calm-zen', label: 'Zen circle', duration: 4800,
      render: (t) => {
        // Slowly-drawing enso (zen circle) behind closed eyes.
        const drawn = clamp(t * 1.3, 0, 1);
        const start = Math.PI * 0.75;
        const end = start + drawn * Math.PI * 1.7;
        const cx = W / 2, cy = CY, r = 60;
        const x1 = cx + Math.cos(start) * r, y1 = cy + Math.sin(start) * r;
        const x2 = cx + Math.cos(end) * r, y2 = cy + Math.sin(end) * r;
        const large = (end - start) > Math.PI ? 1 : 0;
        return (
          <g>
            <path d={`M ${x1} ${y1} A ${r} ${r} 0 ${large} 1 ${x2} ${y2}`}
                  fill="none" stroke="var(--accent)" strokeWidth={6}
                  strokeLinecap="round" opacity={0.55} />
            <g fill="none" stroke="var(--eye)" strokeWidth={10}
               strokeLinecap="round">
              <path d={arcPath(LX, CY + 4, EYE_W - 16, 14)} />
              <path d={arcPath(RX, CY + 4, EYE_W - 16, 14)} />
            </g>
          </g>
        );
      } },

    { id: 'calm-lotus', label: 'Lotus', duration: 4000,
      render: (t) => {
        const breath = (Math.sin(t * TAU - Math.PI / 2) + 1) / 2;
        return (
          <g>
            <g fill="none" stroke="var(--eye)" strokeWidth={11}
               strokeLinecap="round">
              <path d={arcPath(LX, CY, EYE_W - 14, lerp(12, 18, breath))} />
              <path d={arcPath(RX, CY, EYE_W - 14, lerp(12, 18, breath))} />
            </g>
            {/* lotus petals at bottom */}
            <g fill="var(--accent)" opacity={0.7}>
              {[-30, -15, 0, 15, 30].map((deg, i) => {
                const a = deg * Math.PI / 180;
                const cx = W / 2 + Math.sin(a) * 36;
                const cy = H - 36 + (1 - Math.cos(a)) * -10;
                return (
                  <ellipse key={i} cx={cx} cy={cy}
                           rx={5 + breath * 1.5} ry={11 + breath * 2}
                           transform={`rotate(${deg} ${cx} ${cy})`}
                           opacity={0.5 + i * 0.08} />
                );
              })}
            </g>
          </g>
        );
      } },
  ],
};

// =============================================================
// EMOTION: PLAYFUL — 3 variants
// =============================================================
cats.playful = {
  label: 'Playful',
  desc: 'Silly / cheeky.',
  tone: 'rose',
  variants: [
    { id: 'playful-blep', label: 'Tongue blep', duration: 2000,
      render: (t) => {
        const wob = Math.sin(t * TAU * 3) * 2;
        const tongueOut = pulse(t, 0.5, 0.35);
        return (
          <g>
            <Eyes L={{ h: EYE_H * 0.85, y: CY - 4 + wob }}
                  R={{ h: EYE_H * 0.85, y: CY - 4 + wob }} />
            {/* tongue */}
            {tongueOut > 0.05 && (
              <g>
                <ellipse cx={W / 2} cy={H - 24 + tongueOut * 16}
                         rx={12} ry={9 + tongueOut * 4}
                         fill="var(--accent)" />
                <line x1={W/2} y1={H - 30 + tongueOut * 12}
                      x2={W/2} y2={H - 20 + tongueOut * 18}
                      stroke="var(--bg)" strokeWidth={1.5} opacity={0.6} />
              </g>
            )}
          </g>
        );
      } },

    { id: 'playful-tease', label: 'Wink + stick', duration: 2400,
      render: (t) => {
        const close = pulse(t, 0.35, 0.18);
        const stick = pulse(t, 0.5, 0.3);
        return (
          <g>
            {close > 0.5 ? (
              <path d={arcPath(LX, CY + 6, EYE_W - 6, 18, true)}
                    fill="none" stroke="var(--eye)"
                    strokeWidth={12} strokeLinecap="round" />
            ) : (
              <Eye x={LX} y={CY} h={EYE_H * (1 - close * 0.6)} />
            )}
            <Eye x={RX} y={CY} h={EYE_H * 0.95} />
            {stick > 0.05 && (
              <ellipse cx={W / 2 + 8} cy={H - 26 + stick * 8}
                       rx={10} ry={6 + stick * 3} fill="var(--accent)" />
            )}
          </g>
        );
      } },

    { id: 'playful-boop', label: 'Boop', duration: 1800,
      render: (t) => {
        const hit = pulse(t, 0.5, 0.12);
        const sq = 1 - hit * 0.18;
        return (
          <g>
            <Eyes L={{ w: EYE_W * sq, h: EYE_H * sq }}
                  R={{ w: EYE_W * sq, h: EYE_H * sq }} />
            {/* finger boop indicator: dot + radiating rings */}
            {hit > 0.05 && [0, 1, 2].map((i) => (
              <circle key={i} cx={W / 2} cy={CY + 2}
                      r={6 + i * 12 * hit}
                      fill="none" stroke="var(--accent)" strokeWidth={2}
                      opacity={(1 - i * 0.3) * (1 - hit * 0.5)} />
            ))}
            {/* smile */}
            <path d={arcPath(W / 2, H - 30, 50, 10, true)}
                  fill="none" stroke="var(--eye)" strokeWidth={5}
                  strokeLinecap="round" />
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: ALARM — 3 variants
// =============================================================
cats.alarm = {
  label: 'Alarm',
  desc: 'Wake / alert.',
  kind: 'scene',
  tone: 'red',
  variants: [
    { id: 'alarm-bell', label: 'Ringing bell', duration: 800,
      render: (t) => {
        const tilt = Math.sin(t * TAU * 4) * 18;
        const ring = Math.sin(t * TAU * 4);
        return (
          <g>
            <g transform={`translate(${W/2} ${CY - 10}) rotate(${tilt})`}>
              {/* bell body */}
              <path d="M -30 0 Q -30 -38, 0 -42 Q 30 -38, 30 0 L 36 6 L -36 6 Z"
                    fill="var(--eye)" />
              {/* clapper */}
              <circle cx={ring * 3} cy={14} r={5} fill="var(--eye)" />
              {/* top knob */}
              <circle cx={0} cy={-44} r={5} fill="var(--eye)" />
            </g>
            {/* sound waves */}
            {[0, 1].map((i) => {
              const p = ((t * 3) + i * 0.5) % 1;
              return (
                <g key={i} opacity={1 - p}>
                  <path d={`M ${W/2 - 70 - p * 14} ${CY - 10}
                            Q ${W/2 - 80 - p * 14} ${CY}, ${W/2 - 70 - p * 14} ${CY + 10}`}
                        fill="none" stroke="var(--accent)" strokeWidth={3}
                        strokeLinecap="round" />
                  <path d={`M ${W/2 + 70 + p * 14} ${CY - 10}
                            Q ${W/2 + 80 + p * 14} ${CY}, ${W/2 + 70 + p * 14} ${CY + 10}`}
                        fill="none" stroke="var(--accent)" strokeWidth={3}
                        strokeLinecap="round" />
                </g>
              );
            })}
            {/* WAKE UP text */}
            <text x={W/2} y={H - 18} textAnchor="middle"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={12} fontWeight={700}
                  fill="var(--accent)" letterSpacing={4}
                  opacity={0.5 + Math.abs(Math.sin(t * TAU * 4)) * 0.5}>
              WAKE UP
            </text>
          </g>
        );
      } },

    { id: 'alarm-clock', label: 'Alarm clock', duration: 1200,
      render: (t) => {
        const tilt = Math.sin(t * TAU * 3) * 6;
        return (
          <g>
            <g transform={`translate(${W/2} ${CY}) rotate(${tilt})`}>
              {/* clock face */}
              <circle cx={0} cy={0} r={42} fill="none"
                      stroke="var(--eye)" strokeWidth={4} />
              {/* bells on top */}
              <ellipse cx={-30} cy={-38} rx={10} ry={9} fill="var(--eye)" />
              <ellipse cx={30} cy={-38} rx={10} ry={9} fill="var(--eye)" />
              {/* legs */}
              <line x1={-22} y1={36} x2={-28} y2={48}
                    stroke="var(--eye)" strokeWidth={4} strokeLinecap="round" />
              <line x1={22} y1={36} x2={28} y2={48}
                    stroke="var(--eye)" strokeWidth={4} strokeLinecap="round" />
              {/* hour + minute hands rotate */}
              <line x1={0} y1={0} x2={0} y2={-22}
                    stroke="var(--accent)" strokeWidth={3} strokeLinecap="round"
                    transform={`rotate(${t * 720})`} />
              <line x1={0} y1={0} x2={0} y2={-30}
                    stroke="var(--accent)" strokeWidth={2} strokeLinecap="round"
                    transform={`rotate(${t * 360 * 12})`} />
              <circle cx={0} cy={0} r={3} fill="var(--accent)" />
            </g>
            {/* shake lines around clock */}
            {[0, 1, 2, 3].map((i) => {
              const a = i * (Math.PI / 2) + Math.PI / 4;
              const x = W/2 + Math.cos(a) * 70;
              const y = CY + Math.sin(a) * 70;
              const op = Math.sin(t * TAU * 3) > 0 ? 1 : 0.3;
              return (
                <line key={i} x1={x - Math.cos(a) * 8} y1={y - Math.sin(a) * 8}
                      x2={x + Math.cos(a) * 8} y2={y + Math.sin(a) * 8}
                      stroke="var(--accent)" strokeWidth={2.5}
                      strokeLinecap="round" opacity={op} />
              );
            })}
          </g>
        );
      } },

    { id: 'alarm-snooze', label: 'Snooze', duration: 3600,
      render: (t) => {
        const ring = (Math.sin(t * TAU * 3) > 0.7) ? 1 : 0.4;
        return (
          <g>
            {/* small clock at left */}
            <g transform={`translate(80 ${CY})`}>
              <circle cx={0} cy={0} r={28} fill="none"
                      stroke="var(--eye)" strokeWidth={3} opacity={ring} />
              <ellipse cx={-20} cy={-26} rx={7} ry={6} fill="var(--eye)" opacity={ring} />
              <ellipse cx={20} cy={-26} rx={7} ry={6} fill="var(--eye)" opacity={ring} />
              <line x1={0} y1={0} x2={0} y2={-16}
                    stroke="var(--accent)" strokeWidth={2.5} strokeLinecap="round" />
              <line x1={0} y1={0} x2={12} y2={0}
                    stroke="var(--accent)" strokeWidth={2} strokeLinecap="round" />
            </g>
            {/* big SNOOZE Zzz drifting right */}
            {[0, 1, 2].map((i) => {
              const p = ((t + i * 0.33) % 1);
              const size = lerp(14, 32, p);
              const x = 160 + i * 6 + p * 80;
              const y = CY - 10 - p * 40;
              return (
                <text key={i} x={x} y={y} fontFamily="ui-monospace, Menlo, monospace"
                      fontSize={size} fontWeight={700}
                      fill="var(--accent)" opacity={1 - p}>z</text>
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: NOTIFICATION — 3 variants
// =============================================================
cats.notification = {
  label: 'Notification',
  desc: 'New message / alert.',
  kind: 'scene',
  tone: 'warm',
  variants: [
    { id: 'notif-bell', label: 'Bell + badge', duration: 1600,
      render: (t) => {
        const tilt = Math.sin(t * TAU * 6) * 14 * Math.max(0, 1 - t * 1.4);
        const badgePop = t < 0.25 ? ease.out(t / 0.25) : 1;
        return (
          <g>
            <g transform={`translate(${W/2} ${CY + 6}) rotate(${tilt})`}>
              {/* bell */}
              <path d="M -28 0 Q -28 -36, 0 -40 Q 28 -36, 28 0 L 34 6 L -34 6 Z"
                    fill="var(--eye)" />
              <circle cx={0} cy={14} r={5} fill="var(--eye)" />
              <circle cx={0} cy={-42} r={4} fill="var(--eye)" />
            </g>
            {/* badge */}
            <g transform={`translate(${W/2 + 26} ${CY - 30}) scale(${badgePop})`}>
              <circle cx={0} cy={0} r={12} fill="var(--accent)" />
              <text x={0} y={5} textAnchor="middle"
                    fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={14} fontWeight={700} fill="var(--bg)">3</text>
            </g>
          </g>
        );
      } },

    { id: 'notif-popup', label: 'Pop-up card', duration: 2800,
      render: (t) => {
        const slideIn = clamp(t * 4, 0, 1);
        const slideOut = clamp((t - 0.8) * 5, 0, 1);
        const y = lerp(STATUS_H - 50, STATUS_H + 14, ease.out(slideIn)) -
                  slideOut * 64;
        return (
          <g>
            {/* notification card */}
            <g transform={`translate(${W/2 - 110} ${y})`}>
              <rect x={0} y={0} width={220} height={56} rx={10}
                    fill="var(--eye)" opacity={0.95} />
              {/* icon */}
              <circle cx={20} cy={28} r={11} fill="var(--bg)" />
              <text x={20} y={32} textAnchor="middle"
                    fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={12} fontWeight={700} fill="var(--accent)">!</text>
              {/* title line */}
              <rect x={42} y={14} width={140} height={6} rx={3}
                    fill="var(--bg)" opacity={0.85} />
              {/* body lines */}
              <rect x={42} y={28} width={156} height={4} rx={2}
                    fill="var(--bg)" opacity={0.5} />
              <rect x={42} y={38} width={100} height={4} rx={2}
                    fill="var(--bg)" opacity={0.5} />
            </g>
            {/* small eye glimpse behind */}
            <g opacity={0.35}>
              <Eyes L={{ y: CY + 30, h: EYE_H * 0.45 }}
                    R={{ y: CY + 30, h: EYE_H * 0.45 }} />
            </g>
          </g>
        );
      } },

    { id: 'notif-ping', label: 'Ping radar', duration: 1800,
      render: (t) => (
        <g>
          {/* center dot */}
          <circle cx={W/2} cy={CY} r={6} fill="var(--accent)" />
          {/* expanding rings */}
          {[0, 1, 2].map((i) => {
            const p = ((t * 1.5) + i * 0.33) % 1;
            return (
              <circle key={i} cx={W/2} cy={CY}
                      r={8 + p * 70}
                      fill="none" stroke="var(--accent)" strokeWidth={2.5}
                      opacity={(1 - p) * 0.9} />
            );
          })}
          {/* @ symbol below */}
          <text x={W/2} y={H - 18} textAnchor="middle"
                fontFamily="ui-monospace, Menlo, monospace"
                fontSize={14} fontWeight={700} fill="var(--eye)" letterSpacing={3}>
            NEW
          </text>
        </g>
      ) },
  ],
};

// =============================================================
// SCENE: BIRTHDAY — 3 variants
// =============================================================
cats.birthday = {
  label: 'Birthday',
  desc: 'Cake / confetti / candle.',
  kind: 'scene',
  tone: 'rose',
  variants: [
    { id: 'birthday-cake', label: 'Cake', duration: 1800,
      render: (t) => {
        const flick = 0.7 + Math.sin(t * TAU * 6) * 0.3;
        return (
          <g>
            {/* plate */}
            <ellipse cx={W/2} cy={H - 20} rx={88} ry={6}
                     fill="var(--eye)" opacity={0.5} />
            {/* cake tier 1 (bottom) */}
            <rect x={W/2 - 76} y={H - 70} width={152} height={42} rx={4}
                  fill="var(--eye)" />
            {/* frosting drips */}
            <path d={`M ${W/2 - 76} ${H - 60}
                      Q ${W/2 - 60} ${H - 50}, ${W/2 - 44} ${H - 60}
                      T ${W/2 - 12} ${H - 60}
                      T ${W/2 + 20} ${H - 60}
                      T ${W/2 + 52} ${H - 60}
                      T ${W/2 + 76} ${H - 60}`}
                  fill="none" stroke="var(--accent)" strokeWidth={4} />
            {/* tier 2 (top) */}
            <rect x={W/2 - 50} y={H - 110} width={100} height={36} rx={4}
                  fill="var(--eye)" />
            {/* candle */}
            <rect x={W/2 - 3} y={H - 138} width={6} height={28} rx={1}
                  fill="var(--accent)" />
            {/* flame */}
            <ellipse cx={W/2} cy={H - 144} rx={5 * flick} ry={9 * flick}
                     fill="var(--accent)" />
            <ellipse cx={W/2} cy={H - 144} rx={2.5 * flick} ry={5 * flick}
                     fill="var(--eye)" opacity={0.6} />
          </g>
        );
      } },

    { id: 'birthday-confetti', label: 'Confetti', duration: 2400,
      render: (t) => (
        <g>
          {/* HBD text */}
          <text x={W/2} y={CY + 6} textAnchor="middle"
                fontFamily="ui-monospace, Menlo, monospace"
                fontSize={32} fontWeight={800}
                fill="var(--eye)" letterSpacing={4}>
            HBD!
          </text>
          {/* confetti rectangles falling */}
          {Array.from({ length: 18 }).map((_, i) => {
            const seed = (i * 137) % 100 / 100;
            const p = ((t * 1.3) + seed) % 1;
            const x = (i * 41) % (W - 20) + 10;
            const y = lerp(STATUS_H, H - 4, p);
            const rot = p * 360 * (i % 2 ? 1 : -1);
            const tone = (i % 3 === 0) ? 'var(--accent)' :
                         (i % 3 === 1) ? 'var(--eye)' : 'var(--accent)';
            return (
              <g key={i} transform={`translate(${x} ${y}) rotate(${rot})`}>
                <rect x={-3} y={-1} width={6} height={3}
                      fill={tone} opacity={1 - p * 0.3} />
              </g>
            );
          })}
        </g>
      ) },

    { id: 'birthday-balloon', label: 'Balloons', duration: 4200,
      render: (t) => (
        <g>
          {[0, 1, 2].map((i) => {
            const seed = i * 0.31;
            const p = ((t + seed) % 1);
            const baseX = 60 + i * 100;
            const x = baseX + Math.sin(p * TAU + i) * 8;
            const y = lerp(H + 30, STATUS_H + 10, p);
            const color = i === 1 ? 'var(--accent)' : 'var(--eye)';
            return (
              <g key={i} opacity={1 - clamp((p - 0.8) * 5, 0, 1)}>
                {/* balloon */}
                <ellipse cx={x} cy={y} rx={18} ry={22} fill={color} />
                {/* knot */}
                <path d={`M ${x - 3} ${y + 22} L ${x + 3} ${y + 22} L ${x} ${y + 26} Z`}
                      fill={color} />
                {/* string */}
                <path d={`M ${x} ${y + 26}
                          Q ${x + Math.sin(t * TAU * 2 + i) * 5} ${y + 44},
                          ${x} ${y + 60}`}
                      fill="none" stroke={color} strokeWidth={1} opacity={0.6} />
              </g>
            );
          })}
        </g>
      ) },
  ],
};

// =============================================================
// SCENE: REMINDER — 3 variants  (water · medicine · stretch)
// =============================================================
cats.reminder = {
  label: 'Reminder',
  desc: 'Drink water · take meds · stretch.',
  kind: 'scene',
  tone: 'orange',
  variants: [
    { id: 'reminder-water', label: 'Drink water', duration: 2200,
      render: (t) => {
        const fill = (Math.sin(t * TAU - Math.PI / 2) + 1) / 2;
        const fillH = lerp(8, 56, fill);
        return (
          <g>
            <g transform={`translate(${W/2 - 28} ${CY - 36})`}>
              {/* glass outline */}
              <path d="M 0 0 L 4 70 L 52 70 L 56 0 Z"
                    fill="none" stroke="var(--eye)" strokeWidth={4} />
              {/* water */}
              <rect x={4} y={70 - fillH} width={48} height={fillH}
                    fill="var(--accent)" opacity={0.85} />
              {/* surface ripple */}
              <path d={`M 4 ${70 - fillH}
                        Q 16 ${70 - fillH - 2}, 28 ${70 - fillH}
                        T 52 ${70 - fillH}`}
                    fill="none" stroke="var(--eye)" strokeWidth={1.5} opacity={0.7} />
            </g>
            {/* droplet falling */}
            {(t * 2 % 1) < 0.5 && (
              <path d={`M ${W/2} ${STATUS_H + 8 + (t * 2 % 1) * 60}
                        q -5 -7, 0 -12 q 5 5, 0 12 Z`}
                    fill="var(--accent)" />
            )}
            {/* label */}
            <text x={W/2} y={H - 14} textAnchor="middle"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={11} fontWeight={700}
                  fill="var(--eye)" letterSpacing={3}>HYDRATE</text>
          </g>
        );
      } },

    { id: 'reminder-meds', label: 'Take meds', duration: 2400,
      render: (t) => {
        const tilt = Math.sin(t * TAU * 1.5) * 8;
        return (
          <g>
            {/* capsule pill */}
            <g transform={`translate(${W/2} ${CY - 6}) rotate(${tilt})`}>
              <rect x={-40} y={-16} width={80} height={32} rx={16}
                    fill="var(--accent)" />
              <rect x={-40} y={-16} width={40} height={32} rx={16}
                    fill="var(--eye)" />
              {/* gloss */}
              <rect x={-32} y={-12} width={28} height={4} rx={2}
                    fill="var(--bg)" opacity={0.4} />
            </g>
            {/* sparkle/ready dots */}
            {[0, 1, 2].map((i) => {
              const a = i * (TAU / 3) + t * TAU;
              const r = 60;
              const x = W / 2 + Math.cos(a) * r;
              const y = CY + Math.sin(a) * r * 0.6;
              return (
                <circle key={i} cx={x} cy={y} r={3}
                        fill="var(--eye)" opacity={0.7} />
              );
            })}
            <text x={W/2} y={H - 14} textAnchor="middle"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={11} fontWeight={700}
                  fill="var(--eye)" letterSpacing={3}>2 PILLS · 8AM</text>
          </g>
        );
      } },

    { id: 'reminder-stretch', label: 'Stretch', duration: 2400,
      render: (t) => {
        const phase = (Math.sin(t * TAU - Math.PI / 2) + 1) / 2;
        const armSpread = lerp(0, 30, phase);
        return (
          <g>
            {/* stick figure stretching */}
            <g transform={`translate(${W/2} ${CY - 10})`}>
              <circle cx={0} cy={-30} r={12} fill="var(--eye)" />
              <line x1={0} y1={-18} x2={0} y2={26}
                    stroke="var(--eye)" strokeWidth={5} strokeLinecap="round" />
              {/* arms */}
              <line x1={0} y1={-6}
                    x2={-26 - armSpread} y2={-6 - armSpread * 0.4}
                    stroke="var(--eye)" strokeWidth={5} strokeLinecap="round" />
              <line x1={0} y1={-6}
                    x2={26 + armSpread} y2={-6 - armSpread * 0.4}
                    stroke="var(--eye)" strokeWidth={5} strokeLinecap="round" />
              {/* legs */}
              <line x1={0} y1={26} x2={-16} y2={56}
                    stroke="var(--eye)" strokeWidth={5} strokeLinecap="round" />
              <line x1={0} y1={26} x2={16} y2={56}
                    stroke="var(--eye)" strokeWidth={5} strokeLinecap="round" />
            </g>
            {/* motion lines */}
            {phase > 0.5 && [0, 1].map((i) => (
              <g key={i} stroke="var(--accent)" strokeWidth={2}
                 strokeLinecap="round" opacity={(phase - 0.5) * 2}>
                <line x1={W/2 - 70 - i * 8} y1={CY - 20 + i * 12}
                      x2={W/2 - 60 - i * 8} y2={CY - 20 + i * 12} />
                <line x1={W/2 + 70 + i * 8} y1={CY - 20 + i * 12}
                      x2={W/2 + 60 + i * 8} y2={CY - 20 + i * 12} />
              </g>
            ))}
          </g>
        );
      } },
  ],
};

// (Bluetooth merged into the `network` category — see scenes-network.jsx)

// =============================================================
// SCENE: VIDEO — 2 variants (video call · video playing)
// =============================================================
cats.video = {
  label: 'Video',
  desc: 'Video call / playback.',
  kind: 'scene',
  tone: 'cyan',
  variants: [
    { id: 'video-call', label: 'Video call', duration: 2400,
      render: (t) => {
        const ring = 0.6 + Math.abs(Math.sin(t * TAU * 1.5)) * 0.4;
        return (
          <g>
            {/* frame */}
            <rect x={W/2 - 90} y={STATUS_H + 14} width={180} height={H - STATUS_H - 50}
                  rx={6} fill="none" stroke="var(--eye)" strokeWidth={2.5}
                  opacity={ring} />
            {/* avatar inside */}
            <g transform={`translate(${W/2} ${CY - 6})`}>
              <circle cx={0} cy={-14} r={18} fill="var(--eye)" />
              <path d="M -30 26 Q -30 4, 0 4 Q 30 4, 30 26 Z"
                    fill="var(--eye)" />
            </g>
            {/* REC dot */}
            <g opacity={Math.sin(t * TAU * 2) > 0 ? 1 : 0.3}>
              <circle cx={W/2 - 76} cy={STATUS_H + 26} r={4}
                      fill="var(--accent)" />
            </g>
            {/* hangup pill at bottom */}
            <rect x={W/2 - 18} y={H - 28} width={36} height={14} rx={7}
                  fill="var(--accent)" />
          </g>
        );
      } },

    { id: 'video-play', label: 'Playing', duration: 2200,
      render: (t) => {
        const progress = (t * 0.8) % 1;
        return (
          <g>
            {/* frame */}
            <rect x={W/2 - 110} y={STATUS_H + 16} width={220} height={H - STATUS_H - 58}
                  rx={4} fill="var(--eye)" opacity={0.18} />
            {/* play triangle */}
            <path d={`M ${W/2 - 14} ${CY - 16} L ${W/2 - 14} ${CY + 16} L ${W/2 + 18} ${CY} Z`}
                  fill="var(--eye)" />
            {/* progress bar */}
            <rect x={W/2 - 110} y={H - 24} width={220} height={4} rx={2}
                  fill="var(--eye)" opacity={0.3} />
            <rect x={W/2 - 110} y={H - 24} width={220 * progress} height={4} rx={2}
                  fill="var(--accent)" />
            <circle cx={W/2 - 110 + 220 * progress} cy={H - 22} r={4}
                    fill="var(--accent)" />
          </g>
        );
      } },
  ],
};

// =============================================================
// Merge new keys into the public KEYS list
// New emotions go AFTER existing emotions but BEFORE scenes.
// New scenes go at the END.
// =============================================================
(() => {
  const newEmotionKeys = ['sick', 'cold', 'calm', 'playful'];
  const newSceneKeys   = ['alarm', 'notification', 'birthday',
                          'reminder', 'video'];
  const allNew = [...newEmotionKeys, ...newSceneKeys];

  const keys = (window.EMOTION_CATEGORY_KEYS || Object.keys(cats))
                 .filter((k) => !allNew.includes(k));

  // Find first scene index for insertion
  let firstScene = keys.findIndex((k) => cats[k] && cats[k].kind === 'scene');
  if (firstScene === -1) firstScene = keys.length;

  // Insert new emotions just before scenes
  keys.splice(firstScene, 0, ...newEmotionKeys.filter((k) => k in cats));
  // Append new scenes at end
  keys.push(...newSceneKeys.filter((k) => k in cats));

  window.EMOTION_CATEGORY_KEYS = keys;
})();
