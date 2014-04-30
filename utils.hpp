#ifndef UTILS_HPP
#define UTILS_HPP

#include <QString>

inline bool hasExtension(const QString& fileName)
{
	return -1 not_eq fileName.lastIndexOf('.');
} // hasExtension


inline QString withoutExtension(const QString& fileName)
{
	return fileName.left(fileName.lastIndexOf("."));
} // withoutExtension

#endif // UTILS_HPP
