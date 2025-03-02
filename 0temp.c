
#if 0

ThunarFileMode th_file_get_mode(const ThunarFile *file)
{
    e_return_val_if_fail(IS_THUNARFILE(file), 0);

    if (file->gfileinfo == NULL)
        return 0;

    if (g_file_info_has_attribute(file->gfileinfo, G_FILE_ATTRIBUTE_UNIX_MODE))
        return g_file_info_get_attribute_uint32(file->gfileinfo, G_FILE_ATTRIBUTE_UNIX_MODE);
    else
        return th_file_is_directory(file) ? 0777 : 0666;
}

#endif


