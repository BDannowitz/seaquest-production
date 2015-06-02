-- MySQL dump 10.13  Distrib 5.5.41, for debian-linux-gnu (x86_64)
--
-- Host: e906-db3.fnal.gov    Database: merged_roadset57_R004
-- ------------------------------------------------------
-- Server version	5.1.73

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `Spill`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `Spill` (
  `spillID` int(8) unsigned NOT NULL,
  `runID` smallint(5) unsigned NOT NULL,
  `beamIntensity` double unsigned DEFAULT NULL,
  `beamIntensityError` double unsigned DEFAULT NULL,
  `targetPos` tinyint(3) unsigned NOT NULL,
  `dataQuality` bigint(8) unsigned DEFAULT '0',
  `BOScodaEventID` int(10) unsigned NOT NULL,
  `BOSvmeTime` int(8) unsigned NOT NULL,
  `EOScodaEventID` int(10) unsigned DEFAULT NULL,
  `EOSvmeTime` int(8) unsigned DEFAULT NULL,
  `time` datetime DEFAULT NULL,
  KEY `runID` (`runID`) USING BTREE,
  KEY `spillID` (`spillID`) USING BTREE,
  KEY `targetPos` (`targetPos`) USING BTREE,
  KEY `dataQuality` (`dataQuality`) USING BTREE
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-02-24 17:45:38
