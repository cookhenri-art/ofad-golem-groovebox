# Vidéo de présentation (v0.47)

Fabrication de la vidéo « OFAD GOLEM » (2 min 33, FR et EN), sans capture d'écran manuelle :

1. Son : `groovebox_render --script demo.gbx scenario.txt son.wav` (la démo jouée par passages :
   pistes entendues, crossfader), puis fondu et niveau YouTube :
   `ffmpeg -i son.wav -af "afade=t=in:d=1.5,afade=t=out:st=148:d=5,loudnorm=I=-14:TP=-1.5" son-final.wav`.
2. Images : maquette servie sur le port 4323 (`npx vite preview --port 4323` dans ui/), puis
   `node capture.cjs fr` et `node capture.cjs en` (15 images/s par scène).
3. Cartes et légendes : `node cards.cjs` (titre, galeries, téléphones, plateformes, fin, légendes FR / EN).
4. Montage : `python3 render.py fr` et `python3 render.py en` (un fichier par scène, concaténés, son ajouté).

Les scripts travaillent dans /tmp/vid (polices et images copiées dans /tmp/vid/web : voir cards.cjs).
