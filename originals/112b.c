static bool verify_vbr_checksum(struct exfat_dev* dev, void* sector,
		off_t sector_size)
{
	uint32_t vbr_checksum;
	int i;

	if (exfat_pread(dev, sector, sector_size, 0) < 0)
	{
		exfat_error("failed to read boot sector");
		return false;
	}
	vbr_checksum = exfat_vbr_start_checksum(sector, sector_size);
	for (i = 1; i < 11; i++)
	{
		if (exfat_pread(dev, sector, sector_size, i * sector_size) < 0)
		{
			exfat_error("failed to read VBR sector");
			return false;
		}
		vbr_checksum = exfat_vbr_add_checksum(sector, sector_size,
				vbr_checksum);
	}
	if (exfat_pread(dev, sector, sector_size, i * sector_size) < 0)
	{
		exfat_error("failed to read VBR checksum sector");
		return false;
	}
	for (i = 0; i < sector_size / sizeof(vbr_checksum); i++)
		if (le32_to_cpu(((const le32_t*) sector)[i]) != vbr_checksum)
		{
			exfat_error("invalid VBR checksum 0x%x (expected 0x%x)",
					le32_to_cpu(((const le32_t*) sector)[i]), vbr_checksum);
			return false;
		}
	return true;
}
