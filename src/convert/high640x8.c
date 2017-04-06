/*
  Hatari - high640x8.c

  This file is distributed under the GNU Public License, version 2 or at your
  option any later version. Read the file gpl.txt for details.

  NeXT mono, memory to SDL_Surface
*/

static inline void putpixelbw(SDL_Surface * surface, Uint16 x, Uint16 y, Uint32 col)

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


static inline void putpixel(SDL_Surface * surface, Uint16 x, Uint16 y, Uint32 color)

{

    /* Nombre de bits par pixels de la surface d'écran */
    Uint8 bpp = surface->format->BytesPerPixel;
    /* Pointeur vers le pixel à remplacer (pitch correspond à la taille
       d'une ligne d'écran, c'est à dire (longueur * bitsParPixel)
       pour la plupart des cas) */

    Uint8 * p1 = ((Uint8 *)surface->pixels) + y * surface->pitch + x * bpp;


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

static char buffer[832*1152*4];

static void InvalidateScreenBuffer(void) {
    int i;
#if ENABLE_DIMENSION
    if (ConfigureParams.Screen.nMonitorType==MONITOR_TYPE_DIMENSION) {
        for (i = 0; i < (832*1152*4); i++)
            buffer[i] = ND_vram[i]+1;
        return;
    }
#endif
    for (i = 0; i < (832*1152/4); i++) {
        buffer[i] = NEXTVideo[i]+1;
    }
}

static void ConvertHighRes_640x8Bit(void)
{
	int y, x;
	int	col;
	static int first=1;
	int adr;	

	if (first) {
		first=0;
		for (x=0;x<4;x++)
			colors[x] = SDL_MapRGB(sdlscrn->format, sdlColors[x].r, sdlColors[x].g, sdlColors[x].b);
		for (x=0;x<4096;x++)
			hicolors[x]=SDL_MapRGB(sdlscrn->format,((x&0x0F00)>>4)|((x&0x0F00)>>8),(x&0x00F0)|((x&0x00F0)>>4),((x&0x000F)<<4)|(x&0x000F));
	}

#if ENABLE_DIMENSION
    /* dimension */
    if (ConfigureParams.Screen.nMonitorType==MONITOR_TYPE_DIMENSION) {
        for (y = 0; y < 832; y++)
        {
            adr=(y*288*16)+16;
            
            for (x = 0; x < 1120; x++)
            {
                if ((buffer[adr]!=ND_vram[adr]) || (buffer[adr+1]!=ND_vram[adr+1]) || (buffer[adr+2]!=ND_vram[adr+2])) {
                    col=((ND_vram[adr]<<16) | (ND_vram[adr+1]<<8) | (ND_vram[adr+2]));
                    putpixel(sdlscrn,x,y,col);
                    buffer[adr]=ND_vram[adr];
                    buffer[adr+1]=ND_vram[adr+1];
                    buffer[adr+2]=ND_vram[adr+2];
                }
                adr+=4;
            }
        }
        return;
    }
#endif
    
	/* non turbo color */
	if ((ConfigureParams.System.bColor) && (!(ConfigureParams.System.bTurbo)) ){
		for (y = 0; y < 832; y++)
		{
			adr=y*288*8;

			for (x = 0; x < 1120; x++)
			{
			   if ((buffer[adr]!=NEXTColorVideo[adr]) || (buffer[1+adr]!=NEXTColorVideo[1+adr])) {
				col=(  (NEXTColorVideo[adr]<<8) |  (NEXTColorVideo[1+adr])  )>>4;
				putpixel(sdlscrn,x,y,hicolors[col]);
				buffer[adr]=NEXTColorVideo[adr];
				buffer[adr+1]=NEXTColorVideo[adr+1];
				}
			adr+=2;
			}
		}
		return;
	}

	/* turbo color */
	if ((ConfigureParams.System.bColor) && ((ConfigureParams.System.bTurbo))){
		for (y = 0; y < 832; y++)
		{
			adr=y*280*8;

			for (x = 0; x < 1120; x++)
			{
			   if ((buffer[adr]!=NEXTColorVideo[adr]) || (buffer[1+adr]!=NEXTColorVideo[1+adr])) {
				col=(  (NEXTColorVideo[adr]<<8) |  (NEXTColorVideo[1+adr])  )>>4;
				putpixel(sdlscrn,x,y,hicolors[col]);
				buffer[adr]=NEXTColorVideo[adr];
				buffer[adr+1]=NEXTColorVideo[adr+1];
				}
			adr+=2;
			}
		}
		return;
	}
    
	if (ConfigureParams.System.bTurbo) {
		for (y = 0; y < 832; y++)
		{
			adr=y*280;            
			for (x = 0; x < 1120; x+=4)
			{
                if (buffer[adr]!=NEXTVideo[adr]) {
                        col=(NEXTVideo[adr]&0xC0)>>6;
			putpixelbw(sdlscrn,x,y,col);
                        col=(NEXTVideo[adr]&0x30)>>4;
			putpixelbw(sdlscrn,x+1,y,col);
                        col=(NEXTVideo[adr]&0x0C)>>2;
			putpixelbw(sdlscrn,x+2,y,col);
                        col=(NEXTVideo[adr]&0x03);
			putpixelbw(sdlscrn,x+3,y,col);
                    buffer[adr]=NEXTVideo[adr];
                }
			adr+=1;
			}
		}
	}
	else {

		for (y = 0; y < 832; y++)
		{
			adr=y*288;            
			for (x = 0; x < 1120; x+=4)
			{
                if (buffer[adr]!=NEXTVideo[adr]) {
                        col=(NEXTVideo[adr]&0xC0)>>6;
			putpixelbw(sdlscrn,x,y,col);
                        col=(NEXTVideo[adr]&0x30)>>4;
			putpixelbw(sdlscrn,x+1,y,col);
                        col=(NEXTVideo[adr]&0x0C)>>2;
			putpixelbw(sdlscrn,x+2,y,col);
                        col=(NEXTVideo[adr]&0x03);
			putpixelbw(sdlscrn,x+3,y,col);
                    buffer[adr]=NEXTVideo[adr];
                }
			adr+=1;
			}
		}

	}
}
