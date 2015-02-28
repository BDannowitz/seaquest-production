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
-- Table structure for table `jDimuon`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `jDimuon` (
  `dimuonID` int(11) NOT NULL DEFAULT '0',
  `runID` int(11) NOT NULL DEFAULT '0',
  `spillID` int(11) DEFAULT NULL,
  `eventID` int(11) NOT NULL DEFAULT '0',
  `posTrackID` int(11) DEFAULT NULL,
  `negTrackID` int(11) DEFAULT NULL,
  `dx` double DEFAULT NULL,
  `dy` double DEFAULT NULL,
  `dz` double DEFAULT NULL,
  `dpx` double DEFAULT NULL,
  `dpy` double DEFAULT NULL,
  `dpz` double DEFAULT NULL,
  `mass` double DEFAULT NULL,
  `xF` double DEFAULT NULL,
  `xb` double DEFAULT NULL,
  `xt` double DEFAULT NULL,
  `trackZSeparation` double DEFAULT NULL,
  PRIMARY KEY (`runID`,`dimuonID`),
  KEY `spilldimuon` (`spillID`, `dimuonID`),
  KEY `runeventID` (`runID`,`eventID`),
  KEY `spilleventID` (`spillID`,`eventID`),
  KEY `spill_postrack` (`spillID`,`posTrackID`),
  KEY `spill_negtrack` (`spillID`,`negTrackID`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-02-24 17:45:38
