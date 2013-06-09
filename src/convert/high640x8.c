/*
  Hatari - high640x8.c

  This file is distributed under the GNU Public License, version 2 or at your
  option any later version. Read the file gpl.txt for details.

  NeXT mono, memory to SDL_Surface
*/

static inline void putpixel(SDL_Surface * surface, Uint16 x, Uint16 y, Uint32 col)

{

    /* Nombre de bits par pixels de la surface d'écran */
    Uint8 bpp = surface->format->BytesPerPixel;
    /* Pointeur vers le pixel à remplacer (pitch correspond à la taille
       d'une ligne d'écran, c'est à dire (longueur * bitsParPixel)
       pour la plupart des cas) */

    Uint8 * p1 = ((Uint8 *)surface->pixels) + y * surface->pitch + x * bpp;


	Uint32 color = colors[col];

	switch(bpp) {
		case 1:

			*p1 = color;

			break;

		case 2:

			*(Uint16 *)p1 = color;

			break;

		case 3:
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {

				p1[0] = (color >> 16) & 0xff;
				p1[1] = (color >> 8) & 0xff;
				p1[2] = color & 0xff;

			} else {

				p1[0] = color & 0xff;
				p1[1] = (color >> 8) & 0xff;
				p1[2] = (color >> 16) & 0xff;

			}
			break;

		case 4:

			*(Uint32 *)p1 = color;
			break;
		}

}


static void ConvertHighRes_640x8Bit(void)
{
	int y, x;
	int	col;
	static int first=1;
	
	if (first) {
		first=0;
		for (x=0;x<4;x++)
			colors[x] = SDL_MapRGB(sdlscrn->format, sdlColors[x].r, sdlColors[x].g, sdlColors[x].b);
	}

	for (y = 0; y < 832; y++)
	{

		for (x = 0; x < 1120; x++)
		{
			switch (x&0x3)
			{
			case 0x0:
				col=(NEXTVideo[(x/4)+y*288]&0xC0)>>6;
				break;
			case 0x1:
				col=(NEXTVideo[(x/4)+y*288]&0x30)>>4;
				break;
			case 0x2:
				col=(NEXTVideo[(x/4)+y*288]&0x0C)>>2;
				break;
			case 0x3:
				col=(NEXTVideo[(x/4)+y*288]&0x03);
				break;
			}
            /* Hack to provide video output on color systems to  *
             * do memory configuration experiments. Remove later */
            if (ConfigureParams.System.bColor) {
                col = (NEXTColorVideo[(x*2)+(y*288*8)]&0x30)>>4;
            }
            /* --------------------------------------------------*/
			putpixel(sdlscrn,x,y,col);
		}
	}
}
