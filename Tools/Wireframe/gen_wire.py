# -*- coding: utf-8 -*-
# Room sample wireframe generator (data = placements done 2026-11-25 in LV_Room_* samples)
#
# 사용법 (저장소 루트에서):
#   python Tools/Wireframe/gen_wire.py 와이어프레임_방샘플.html 와이어프레임_방샘플_figma.svg
# - 첫 번째 출력: 브라우저용 HTML, 두 번째: Figma에 끌어다 놓는 SVG(스타일 인라인, 화살촉은 삼각형, 방마다 그룹).
# - 방 데이터(벽·문·엄폐물·적·동선)는 아래 room(...) 호출에 손으로 적혀 있다. 에디터 배치를 바꾸면 여기 좌표도 고칠 것.
# - 좌표는 방 샘플 레벨의 로컬 cm. 화면은 X·Y 모두 뒤집어 입구가 아래, 출구가 위로 오게 그린다(1px = 4cm).
import sys

S = 0.25  # px per cm
COL = {'R': '#FF6B6B', 'M': '#0A96FF', 'B': '#6EEB8C', 'Y': '#FFE14D', 'C': '#E0249A'}
ENTRY, EXIT = '#0A96FF', '#FF6B6B'
P_RED, E_YEL, P_PUR, ENG = '#FF6B6B', '#F2D43A', '#6B6BFF', '#F2D43A'

out = []

def room(title, ox, oy, b, floors, walls, doors, covers, enemies, paths, engs, notes=()):
    """b=(xmin,xmax,ymin,ymax) in room cm. Screen is mirrored on both axes
    (entry at bottom, exit at top, same as the hand-drawn template)."""
    xmin, xmax, ymin, ymax = b
    def P(x, y):
        return ox + (xmax - x) * S, oy + (ymax - y) * S
    def R(x0, x1, y0, y1):
        ax, ay = P(x1, y1); bx, by = P(x0, y0)
        return ax, ay, bx - ax, by - ay
    g = [f'<g id="{title}">', f'<text x="{ox}" y="{oy-14}" class="t">{title}</text>']
    if floors:  # polygon outline
        pts = ' '.join('%.1f,%.1f' % P(x, y) for x, y in floors)
        g.append(f'<polygon points="{pts}" class="room"/>')
    for (x0, x1, y0, y1) in walls:
        x, y, w, h = R(x0, x1, y0, y1)
        g.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" class="cover"/>')
    for (k, x, y, rot) in covers:
        if k == 'L':
            hw, hh = (100, 30) if rot == 0 else (30, 100)
        elif k == 'P':
            hw, hh = 40, 40
        else:  # explicit (w,h)
            hw, hh = k[0] / 2, k[1] / 2
        x, y, w, h = R(x - hw, x + hw, y - hh, y + hh)
        g.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" class="cover{" pillar" if k == "P" else ""}"/>')
    for (kind, x, y, x2, y2) in doors:
        a = P(x, y); c = P(x2, y2)
        col = ENTRY if kind == 'in' else EXIT
        g.append(f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{c[0]:.1f}" y2="{c[1]:.1f}" stroke="{col}" stroke-width="13"/>')
    for (cx, cy, rx, ry, label) in engs:
        x, y = P(cx, cy)
        g.append(f'<ellipse cx="{x:.1f}" cy="{y:.1f}" rx="{rx*S:.1f}" ry="{ry*S:.1f}" class="eng"/>')
        if label:
            g.append(f'<text x="{x:.1f}" y="{y-ry*S*0.55:.1f}" class="s" text-anchor="middle">{label}</text>')
    for (col, pts, label) in paths:
        c = {'r': P_RED, 'y': E_YEL, 'p': P_PUR}[col]
        d = ' '.join('%.1f,%.1f' % P(x, y) for x, y in pts)
        g.append(f'<polyline points="{d}" fill="none" stroke="{c}" stroke-width="1.4" marker-end="url(#a{col})"/>')
        g.append(arrowhead(P(*pts[-2]), P(*pts[-1]), c))
        if label:
            x, y = P(*label[0])
            g.append(f'<text x="{x:.1f}" y="{y:.1f}" class="s" text-anchor="middle">{label[1]}</text>')
    for (t, x, y, w, sub) in enemies:
        px, py = P(x, y)
        g.append(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="11" fill="{COL[t]}"/>')
        g.append(f'<text x="{px:.1f}" y="{py+3.5:.1f}" class="n">{w}</text>')
        if sub:
            g.append(f'<text x="{px:.1f}" y="{py+24:.1f}" class="sub">{sub}</text>')
    for (x, y, txt) in notes:
        px, py = P(x, y)
        g.append(f'<text x="{px:.1f}" y="{py:.1f}" class="s" text-anchor="middle">{txt}</text>')
    g.append('</g>')
    out.append('\n'.join(g))

def arrowhead(a, b, c, L=8, W=4):
    import math
    dx, dy = b[0]-a[0], b[1]-a[1]; n = math.hypot(dx, dy) or 1
    ux, uy = dx/n, dy/n
    bx, by = b[0]-ux*L, b[1]-uy*L
    pts = [(b[0], b[1]), (bx-uy*W, by+ux*W), (bx+uy*W, by-ux*W)]
    return '<polygon class="ah" fill="%s" points="%s"/>' % (c, ' '.join('%.1f,%.1f' % q for q in pts))

def box(xmin, xmax, ymin, ymax):
    return [(xmin, ymin), (xmax, ymin), (xmax, ymax), (xmin, ymax)]

A, E_, M_ = '안드로이드', '엘리트', '메크'

# --- Start ---
room('시작방 / LV_Room_Start', 600, 75, (-500, 500, 0, 1100), box(-500, 500, 0, 1100), [],
     [('out', -200, 1100, 200, 1100)], [], [],
     [('p', [(0, 550), (0, 1080)], None)], [],
     notes=[(0, 520, '중앙에서 플레이어블 캐릭터 생성')])

# --- Small ---
room('일반 작은 방 1 / LV_Room_Small', 920, 75, (-550, 550, 0, 1100), box(-550, 550, 0, 1100), [],
     [('in', -200, 0, 200, 0), ('out', -200, 1100, 200, 1100)],
     [('L', -280, 400, 0), ('L', 280, 400, 0), ('P', 0, 600, 0), ('L', -180, 800, 0)],
     [('B', -400, 750, 1, A), ('B', 400, 750, 1, A), ('R', 0, 900, 2, None), ('B', -300, 980, 2, A)],
     [('r', [(0, 20), (0, 250)], None), ('p', [(-120, 480), (-120, 1000), (-40, 1080)], None)],
     [(0, 400, 380, 200, None)])

# --- Wide ---
room('일반 넓은 방 1 / LV_Room_Wide', 1265, 75, (-650, 650, 0, 1100), box(-650, 650, 0, 1100), [],
     [('in', -200, 0, 200, 0), ('out', 150, 1100, 550, 1100)],
     [('L', -350, 450, 0), ('L', 350, 450, 0), ('P', 0, 600, 0), ('P', 250, 720, 0), ('L', -200, 700, 0)],
     [('B', -450, 800, 1, A), ('B', 450, 850, 1, A), ('B', 0, 900, 1, E_),
      ('C', -500, 1000, 2, None), ('C', 500, 1000, 2, None), ('C', 0, 1020, 2, None)],
     [('r', [(0, 20), (0, 220)], None), ('y', [(-500, 990), (-250, 500)], None), ('y', [(500, 990), (250, 500)], None),
      ('y', [(0, 1000), (-80, 560)], None), ('p', [(80, 560), (350, 1080)], None)],
     [(0, 380, 420, 180, None)])

# --- R01 ---
room('일반 방 1 / LV_Room_R01', 1665, 75, (-550, 550, 20, 1110), box(-550, 550, 20, 1110), [],
     [('in', -200, 20, 200, 20), ('out', -550, 360, -550, 760)],
     [((50, 450), 220, 440, 0), ('L', 0, 680, 0), ('P', -300, 650, 0), ('L', -320, 300, 0)],
     [('B', -400, 910, 1, A), ('B', 400, 910, 1, A), ('B', 0, 960, 2, M_),
      ('C', -250, 1000, 2, None), ('C', 250, 1000, 2, None)],
     [('r', [(0, 40), (80, 250)], None), ('y', [(0, 945), (0, 760)], None),
      ('y', [(-250, 985), (-150, 560)], None), ('y', [(250, 985), (200, 560)], None),
      ('p', [(-200, 480), (-530, 560)], None)],
     [(30, 450, 300, 190, None)])

# --- R02 ---
room('일반 방 2 / LV_Room_R02', 600, 470, (-1700, 550, 20, 1320),
     [(-540, 20), (550, 20), (550, 1320), (-540, 1320), (-540, 820), (-1700, 820), (-1700, 420), (-540, 420)], [],
     [('in', -200, 20, 200, 20), ('out', -1100, 420, -1100, 820)],
     [('L', -250, 500, 0), ('L', 200, 450, 0), ('P', 0, 850, 0), ('L', 300, 950, 0)],
     [('B', -300, 1120, 1, A), ('B', 300, 1120, 1, A), ('B', 400, 670, 2, M_), ('R', -400, 700, 2, None)],
     [('r', [(0, 40), (0, 450)], None), ('y', [(385, 670), (150, 640)], None),
      ('p', [(-260, 650), (-1080, 620)], None)],
     [(0, 650, 270, 230, None)])

# --- Large ---
room('큰 방 / LV_Room_Large', 1435, 470, (-1000, 1000, 0, 1600), box(-1000, 1000, 0, 1600), [],
     [('in', -200, 0, 200, 0), ('out', -800, 1600, -400, 1600)],
     [('L', -450, 650, 0), ('L', 450, 650, 0), ('L', 0, 1150, 0),
      ('P', -250, 420, 0), ('P', 250, 420, 0), ('L', -750, 500, 90), ('L', 750, 500, 90)],
     [('R', -650, 1250, 1, None), ('R', 650, 1250, 1, None), ('B', 0, 1000, 1, A),
      ('B', 0, 1350, 2, M_), ('C', -750, 900, 2, None), ('C', 750, 900, 2, None)],
     [('r', [(0, 20), (0, 300)], None), ('y', [(-735, 885), (-480, 760)], None), ('y', [(735, 885), (480, 760)], None),
      ('y', [(0, 1330), (0, 1180)], None), ('p', [(-400, 980), (-600, 1580)], None)],
     [(0, 700, 820, 290, None)])

# --- Maze ---
room('미로 방 / LV_Room_Maze', 600, 1070, (-1375, 1375, 0, 1100), box(-1375, 1375, 0, 1100),
     [(-1377, 767, 360, 400), (-737, 1407, 740, 780)],
     [('in', -200, 0, 200, 0), ('out', -200, 1100, 200, 1100)],
     [('L', -700, 200, 0), ('L', 200, 570, 0), ('L', 1000, 200, 0)],
     [('C', -1100, 610, 1, None), ('C', 720, 960, 1, None), ('B', -300, 570, 1, A),
      ('B', 1250, 950, 2, M_), ('C', -1100, 920, 2, None)],
     [('r', [(0, 20), (900, 180), (1050, 520), (500, 580)], None),
      ('y', [(-1085, 610), (-150, 560)], None),
      ('y', [(705, 960), (-1050, 940), (-1050, 620), (-600, 590)], None),
      ('r', [(-50, 560), (-1000, 560), (-1050, 870)], (( -700, 620), '1차 교전 종료 후')),
      ('y', [(1235, 950), (-780, 930)], None),
      ('p', [(-600, 960), (-150, 1080)], None)],
     [(350, 570, 330, 130, '1차 교전'), (-850, 920, 300, 120, '2차 교전')])

LEGEND = '''
<rect x="10" y="10" width="515" height="1340" class="room"/>
<g id="범례" class="s">
<text x="35" y="45">초기 기획 단계에서 한 스테이지 플레이 타임을 다음과 같이 잡음</text>
<text x="40" y="62">• 스테이지 7~8분(560~640초) 가량</text>
<text x="40" y="78">• 일반방 30~60초 가량</text>
<text x="40" y="94">• 보스방 120초 가량</text>
<text x="35" y="125">따라서 해당 플레이 타임을 기준으로 방 개수를 산정하면 다음과 같음</text>
<text x="35" y="141">(일반방 평균 45초로 가정)</text>
<text x="35" y="172">일반방 10개 보스방 1개</text>
<text x="35" y="203">플레이어에게 매 번 다른 다양한 환경에 대응하는 경험을 제공하기 위해</text>
<text x="35" y="219">레벨은 풀에 담긴 것을 랜덤으로 호출하는 형태로 구현</text>
<text x="35" y="235">랜덤 레벨의 초기 종류는 우선 15개로 시험 제작</text>
<text x="35" y="251">이후 종류를 늘려야 할 수요가 발생하면 추가 작업 예정</text>
<text x="35" y="300">입구</text><rect x="35" y="307" width="100" height="14" fill="#0A96FF"/>
<text x="35" y="365">출구</text><rect x="35" y="372" width="100" height="14" fill="#FF6B6B"/>
<text x="35" y="430">지형지물 (낮은 벽 200×60×120)</text><rect x="35" y="437" width="100" height="14" class="cover"/>
<text x="35" y="475">기둥 (80×80×260)</text><rect x="35" y="482" width="20" height="20" class="cover pillar"/>
</g>
'''
ENEMY_LEG = [('R', '원거리 적'), ('B', '로봇 적 (아래 글자: 안드로이드 / 엘리트 / 메크)'), ('C', '크롤러 적')]
leg = [LEGEND]
for i, (k, t) in enumerate(ENEMY_LEG):
    y = 545 + i * 44
    leg.append(f'<circle cx="47" cy="{y}" r="12" fill="{COL[k]}"/><text x="47" y="{y+4}" class="n">0</text><text x="72" y="{y+4}" class="s">{t}</text>')
leg.append('''<g class="s">
<text x="35" y="697">적 아이콘 안에 적힌 숫자가 등장 웨이브의 숫자임</text>
<text x="35" y="732">맵 이름 작성 규칙: 맵 이름 / 레벨 파일명</text>
<text x="35" y="748">예시) 일반방01 / LV_Room_NormalRoom01</text>
<text x="35" y="807">예상 동선 표시</text>
<polygon class="ah" fill="#FF6B6B" points="135,832 127,828 127,836"/><line x1="35" y1="832" x2="135" y2="832" stroke="#FF6B6B" stroke-width="1.4" marker-end="url(#ar)"/><text x="150" y="836">(교전 시작 전)유저 예상 경로</text>
<polygon class="ah" fill="#F2D43A" points="135,872 127,868 127,876"/><line x1="35" y1="872" x2="135" y2="872" stroke="#F2D43A" stroke-width="1.4" marker-end="url(#ay)"/><text x="150" y="876">(교전 시작 전)적 예상 경로</text>
<circle cx="70" cy="917" r="14" class="eng"/><text x="150" y="921">예상 교전지</text>
<polygon class="ah" fill="#6B6BFF" points="135,962 127,958 127,966"/><line x1="35" y1="962" x2="135" y2="962" stroke="#6B6BFF" stroke-width="1.4" marker-end="url(#ap)"/><text x="150" y="966">(교전 종료 후)유저 예상 경로</text>
<text x="35" y="1022">※ 2026-11-25 실제 배치(LV_Room_* 샘플) 기준. 좌표 1px = 4cm.</text>
<text x="35" y="1038">※ 보스방(LV_Room_Boss, LV_Room_BigBossRoom)은 변경 없음.</text>
</g>''')

html = f'''<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><title>방 샘플 와이어프레임</title>
<style>
body{{margin:0;background:#fff;font-family:"Pretendard","Malgun Gothic",sans-serif}}
.room{{fill:none;stroke:#222;stroke-width:1.3}}
.cover{{fill:#fff;stroke:#222;stroke-width:1.2}}
.pillar{{fill:#ddd}}
.eng{{fill:none;stroke:#F2D43A;stroke-width:1.6}}
.t{{font-size:19px;fill:#222}}
.s{{font-size:11px;fill:#222}}
.n{{font-size:10px;fill:#fff;text-anchor:middle;font-weight:600}}
.sub{{font-size:9px;fill:#2a8a45;text-anchor:middle}}
</style></head><body>
<svg xmlns="http://www.w3.org/2000/svg" width="2080" height="1380" viewBox="0 0 2080 1380">
<defs>
<marker id="ar" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><path d="M0,1 L7,4 L0,7" fill="none" stroke="#FF6B6B"/></marker>
<marker id="ay" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><path d="M0,1 L7,4 L0,7" fill="none" stroke="#F2D43A"/></marker>
<marker id="ap" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><path d="M0,1 L7,4 L0,7" fill="none" stroke="#6B6BFF"/></marker>
</defs>
{''.join(leg)}
{chr(10).join(out)}
</svg></body></html>'''
open(sys.argv[1], 'w', encoding='utf-8').write(html.replace('<polygon class="ah"', '<polygon style="display:none" class="ah"'))

# Figma용 SVG: 클래스 스타일을 속성으로 풀고, 화살표는 marker 대신 삼각형, 방마다 그룹
import re
FONT = 'font-family="Noto Sans KR"'
INL = {
 'room': 'fill="none" stroke="#222222" stroke-width="1.3"',
 'cover': 'fill="#FFFFFF" stroke="#222222" stroke-width="1.2"',
 'cover pillar': 'fill="#DDDDDD" stroke="#222222" stroke-width="1.2"',
 'eng': 'fill="none" stroke="#F2D43A" stroke-width="1.6"',
 't': FONT + ' font-size="19" fill="#222222"',
 's': FONT + ' font-size="11" fill="#222222"',
 'n': FONT + ' font-size="10" font-weight="700" fill="#FFFFFF" text-anchor="middle"',
 'sub': FONT + ' font-size="9" fill="#2A8A45" text-anchor="middle"',
 'ah': '',
}
body = html[html.index('<svg'):html.index('</svg>')+6]
body = re.sub(r'<defs>.*?</defs>', '', body, flags=re.S)
body = re.sub(r' marker-end="url[^"]*"', '', body)
body = re.sub(r'class="([^"]+)"', lambda m: INL[m.group(1)], body)

body = body.replace('viewBox="0 0 2080 1380">', 'viewBox="0 0 2080 1380"><rect width="2080" height="1380" fill="#FFFFFF"/>', 1)
open(sys.argv[2], 'w', encoding='utf-8').write('<?xml version="1.0" encoding="UTF-8"?>' + chr(10) + body)
print('ok')
