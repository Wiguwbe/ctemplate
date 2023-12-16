
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

#include "common.h"
#include "objcopy.h"

int objcopy(uint8_t *input, char *basename)
{
	// elf header
	Elf64_Ehdr elf_header;
	// no program header
	// 6 sect headers (NULL, .data, .symtab, .strtab, .shstrtab, .note.GNU-stack)
	Elf64_Shdr sect_header[6];
	char fname[128];
	// symtab is only 2 symbols (null and _data_{basename})
	Elf64_Sym symtab[2];
	int symtab_len = 2*sizeof(Elf64_Sym);
	int symtab_align = 0;
	// strtab for symtab
	uint8_t strtab[128];
	int strtab_len;
	// shstrtab (null, .data, .symtab, .strtab, .shstrtab, .note.GNU-stack)
	uint8_t *shstrtab = "\0.symtab\0.strtab\0.shstrtab\0.data\0.note.GNU-stack\0";
	int shstrtab_len = 49;
	int data_len = strlen(input);
	int status_code = 1;

	if(snprintf(fname, 128, "tpl-%s-data.o", basename) > 128) {
		fprintf(stderr, "filename would be too long\n");
		return 1;
	}
	if((strtab_len = snprintf(strtab+1, 127, "_data_%s", basename)) > 126) {
		fprintf(stderr, "objcopy: not enough memory for snprintf\n");
		return 1;
	}

	// to include the NULL byte
	// and the beggining of the file
	*strtab = 0;
	strtab_len+=2;

	/*
		0x00
		ELF Header

		0x40
		Sect Headers

		0x40 + 6*sizeof(Shdr)
		.data

		0x40 + 6*sizeof(Shdr)+align(8)
		.symtab

		0x40 + 6*sizeof(Shdr)+align + 2*sizeof(Sym)
		.strtab

		0x40 + 6*sizeof(Shdr)+align(8) + 2*sizeof(Sym)+len(strtab)
		.shstrtab
	*/

	// prepare headers
	{ /* elf header */
		memset(&elf_header, 0, sizeof elf_header);
		elf_header.e_ident[0] = ELFMAG0;
		elf_header.e_ident[1] = ELFMAG1;
		elf_header.e_ident[2] = ELFMAG2;
		elf_header.e_ident[3] = ELFMAG3;
		elf_header.e_ident[4] = ELFCLASS64;
		elf_header.e_ident[5] = ELFDATA2LSB;
		elf_header.e_ident[6] = EV_CURRENT;
		elf_header.e_ident[7] = ELFOSABI_SYSV;
		// rest is 0
		elf_header.e_type = ET_REL;
		elf_header.e_machine = EM_X86_64;
		elf_header.e_version = EV_CURRENT;
		elf_header.e_entry = 0;
		// program header
		elf_header.e_phoff = 0;
		elf_header.e_shoff = 0x40;	// TODO later
		elf_header.e_flags = 0;
		elf_header.e_ehsize = sizeof elf_header;
		elf_header.e_phentsize = sizeof(Elf64_Phdr);	// not relevant tho
		elf_header.e_phnum = 0;
		elf_header.e_shentsize = sizeof(Elf64_Shdr);
		elf_header.e_shnum = 6;
		elf_header.e_shstrndx = 4;	// last one
	}

	/*
		section headers
	*/
	{ /* NULL header */
		Elf64_Shdr *hdr = sect_header;
		hdr->sh_name = 0;	// NULL
		hdr->sh_type = SHT_NULL;
		hdr->sh_flags = 0;
		hdr->sh_addr = 0;
		hdr->sh_offset = 0;
		hdr->sh_size = 0;
		hdr->sh_link = 0;
		hdr->sh_info = 0;
		hdr->sh_addralign = 0;
		hdr->sh_entsize = 0;
	}
	{ /* .data header */
		Elf64_Shdr *hdr = sect_header+1;
		hdr->sh_name = 27;	// .data
		hdr->sh_type = SHT_PROGBITS;
		hdr->sh_flags = SHF_WRITE|SHF_ALLOC;
		hdr->sh_addr = 0;
		hdr->sh_offset = 0x40 + 6 * sizeof(Elf64_Shdr);
		hdr->sh_size = data_len;
		hdr->sh_link = 0;
		hdr->sh_info = 0;
		hdr->sh_addralign = 1;
		hdr->sh_entsize = 0;
	}
	{ /* .symtab header */
		Elf64_Shdr *hdr = sect_header+2;
		hdr->sh_name = 1;
		hdr->sh_type = SHT_SYMTAB;
		hdr->sh_flags = 0;
		hdr->sh_addr = 0;
		Elf64_Off offset = (hdr-1)->sh_offset + data_len;
		// align to 8
		if(offset&0x7) {
			symtab_align = 8-(offset&0x7);
			offset = offset+symtab_align;
		}
		hdr->sh_offset = offset;
		hdr->sh_size = symtab_len;
		hdr->sh_link = 3;	// ?
		hdr->sh_info = 1;	// ?
		hdr->sh_addralign = 8;
		hdr->sh_entsize = sizeof(Elf64_Sym);
	}
	{ /* .strtab header */
		Elf64_Shdr *hdr = sect_header+3;
		hdr->sh_name = 9;	// .strtab
		hdr->sh_type = SHT_STRTAB;
		hdr->sh_flags = 0;
		hdr->sh_addr = 0;
		hdr->sh_offset = (hdr-1)->sh_offset + symtab_len;
		hdr->sh_size = strtab_len;
		hdr->sh_link = 0;
		hdr->sh_info = 0;
		hdr->sh_addralign = 1;
		hdr->sh_entsize = 0;
	}
	{ /* .shstrtab header */
		Elf64_Shdr *hdr = sect_header+4;
		hdr->sh_name = 17;
		hdr->sh_type = SHT_STRTAB;
		hdr->sh_flags = 0;
		hdr->sh_addr = 0;
		hdr->sh_offset = (hdr-1)->sh_offset + strtab_len;
		hdr->sh_size = shstrtab_len;
		hdr->sh_link = 0;
		hdr->sh_info = 0;
		hdr->sh_addralign = 1;
		hdr->sh_entsize = 0;
	}
	{ /* .note.GNU-stack header */
		Elf64_Shdr *hdr = sect_header+5;
		hdr->sh_name = 33;
		hdr->sh_type = SHT_PROGBITS;
		hdr->sh_flags = 0;
		hdr->sh_addr = 0;
		hdr->sh_offset = (hdr-1)->sh_offset + shstrtab_len;
		hdr->sh_size = 0;
		hdr->sh_link = 0;
		hdr->sh_info = 0;
		hdr->sh_addralign = 1;
		hdr->sh_entsize = 0;
	}

	{ /* symtab */
		// null entry
		Elf64_Sym *sy = symtab;
		memset(sy, 0, sizeof(Elf64_Sym));
		// _data_<> entry
		sy++;
		sy->st_name = 1;
		sy->st_value = 0;	// points to the beginning
		sy->st_size = 0;
		sy->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
		sy->st_other = STV_DEFAULT;
		sy->st_shndx = 1;	// address to .data
	}

	{ /* write to disk */
		FILE *outf;
		if((outf = fopen(fname, "wb")) == NULL) {
			PERROR(fopen);
			return 1;
		}

		// elf header
		if(fwrite(&elf_header, sizeof elf_header, 1, outf) != 1) {
			PERROR(fwrite);
			fclose(outf);
			return 1;
		}
		// section headers
		if(fwrite(sect_header, sizeof(Elf64_Shdr), 6, outf) != 6) {
			PERROR(fwrite);
			fclose(outf);
			return 1;
		}
		// data
		if(fwrite(input, 1, data_len, outf) != data_len) {
			PERROR(fwrite);
			fclose(outf);
			return 1;
		}
		// align
		if(symtab_align) {
			uint8_t *align = "\0\0\0\0\0\0\0";
			if(fwrite(align, 1, symtab_align, outf) != symtab_align) {
				PERROR(fwrite);
				fclose(outf);
				return 1;
			}
		}
		// symtab
		if(fwrite(symtab, sizeof(Elf64_Sym), 2, outf) != 2) {
			PERROR(fwrite);
			fclose(outf);
			return 1;
		}
		// strtab
		if(fwrite(strtab, 1, strtab_len, outf) != strtab_len) {
			PERROR(fwrite);
			fclose(outf);
			return 1;
		}
		// shstrtab
		if(fwrite(shstrtab, 1, shstrtab_len, outf) != shstrtab_len) {
			PERROR(fwrite);
			fclose(outf);
			return 1;
		}

		if(fclose(outf) == EOF) {
			PERROR(fclose);
			return 1;
		}
	}

	// all good then

	return 0;
}
